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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "db.h"
#include "fts.h"
#include "tokenize.h"

const char *dbpath;

static void __dead
usage(void)
{
	fprintf(stderr, "usage: %s [-d db] -l | -s | query",
	    getprogname());
	exit(1);
}

static int
print_entry(struct db *db, struct db_entry *entry, void *data)
{
	printf("%-18s %s\n", entry->name, entry->descr);
	return 0;
}

int
main(int argc, char **argv)
{
	struct db db;
	const char *errstr;
	int fd, ch;
	int list = 0, stats = 0, docid = -1;

	while ((ch = getopt(argc, argv, "d:lp:s")) != -1) {
		switch (ch) {
		case 'd':
			dbpath = optarg;
			break;
		case 'l':
			list = 1;
			break;
		case 'p':
			docid = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "document id is %s: %s", errstr,
				    optarg);
			break;
		case 's':
			stats = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (dbpath == NULL)
		dbpath = "db";

	if (list && stats)
		usage();

	if ((fd = open(dbpath, O_RDONLY)) == -1)
		err(1, "can't open %s", dbpath);

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	if (db_open(&db, fd) == -1)
		err(1, "db_open");

	if (list) {
		if (db_listall(&db, print_entry, NULL) == -1)
			err(1, "db_listall");
	} else if (stats) {
		struct db_stats st;

		if (db_stats(&db, &st) == -1)
			err(1, "db_stats");
		printf("unique words = %zu\n", st.nwords);
		printf("documents    = %zu\n", st.ndocs);
		printf("longest word = %s\n", st.longest_word);
		printf("most popular = %s (%zu)\n", st.most_popular,
		    st.most_popular_ndocs);
	} else if (docid != -1) {
		struct db_entry e;

		if (db_doc_by_id(&db, docid, &e) == -1)
			errx(1, "failed to fetch document #%d", docid);
		print_entry(&db, &e, NULL);
	} else {
		if (argc != 1)
			usage();
		if (fts(&db, *argv, print_entry, NULL) == -1)
			errx(1, "fts failed");
	}

	db_close(&db);
	close(fd);
}
