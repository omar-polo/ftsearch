/*
 * Copyright (c) 2022 Omar Polo <op@omarpolo.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "db.h"
#include "dictionary.h"

#include "mkftsidx.h"

enum {
	MODE_FILES,
	MODE_SQLPORTS,
	MODE_WIKI,
};

char *
xstrdup(const char *s)
{
	char *t;

	if (s == NULL)
		return NULL;

	if ((t = strdup(s)) == NULL)
		err(1, "strdup");
	return t;
}

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s [-o dbpath] [-m f|p|w] [file ...]\n",
	    getprogname());
	exit(1);
}

int
main(int argc, char **argv)
{
	struct dictionary dict;
	struct db_entry *entries = NULL;
	const char *dbpath = NULL;
	FILE *fp;
	size_t i, len = 0;
	int ch, r = 0, mode = MODE_SQLPORTS;

#ifndef PROFILE
	/* sqlite needs flock */
	if (pledge("stdio rpath wpath cpath flock", NULL) == -1)
		err(1, "pledge");
#endif

	while ((ch = getopt(argc, argv, "m:o:")) != -1) {
		switch (ch) {
		case 'm':
			switch (*optarg) {
			case 'f':
				mode = MODE_FILES;
				break;
			case 'p':
				mode = MODE_SQLPORTS;
				break;
			case 'w':
				mode = MODE_WIKI;
				break;
			default:
				usage();
			}
			break;
		case 'o':
			dbpath = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (dbpath == NULL)
		dbpath = "db";

	if (!dictionary_init(&dict))
		err(1, "dictionary_init");

	if (mode == MODE_FILES)
		r = idx_files(&dict, &entries, &len, argc, argv);
	else if (mode == MODE_SQLPORTS)
		r = idx_ports(&dict, &entries, &len, argc, argv);
	else
		r = idx_wiki(&dict, &entries, &len, argc, argv);

	if (r == 0) {
		if ((fp = fopen(dbpath, "w+")) == NULL)
			err(1, "can't open %s", dbpath);
		if (db_create(fp, &dict, entries, len) == -1) {
			warn("db_create");
			unlink(dbpath);
			r = 1;
		}
		fclose(fp);
	}

	for (i = 0; i < len; ++i) {
		free(entries[i].name);
		free(entries[i].descr);
	}
	free(entries);
	dictionary_free(&dict);

	return r;
}
