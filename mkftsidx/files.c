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

#include <sys/mman.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "db.h"
#include "dictionary.h"
#include "tokenize.h"

#include "mkftsidx.h"

static int
pfile(struct dictionary *dict, struct db_entry **entries, size_t *len,
    size_t *cap, const char *path)
{
	char **toks;
	int fd;
	off_t end;
	void *m;

	if (*len == *cap) {
		size_t newcap;
		void *t;

		newcap = *cap * 1.5;
		if (newcap == 0)
			newcap = 8;
		t = recallocarray(*entries, *cap, newcap, sizeof(**entries));
		if (t == NULL)
			err(1, "recallocarray");
		*cap = newcap;
		*entries = t;
	}

	if ((fd = open(path, O_RDONLY)) == -1) {
		warnx("can't open %s", path);
		return 0;
	}

	if ((end = lseek(fd, 0, SEEK_END)) == -1)
		err(1, "lseek %s", path);

	end++;
	m = mmap(NULL, end, PROT_READ, MAP_PRIVATE, fd, 0);
	if (m == MAP_FAILED)
		err(1, "can't mmap %s", path);

	(*entries)[(*len)++].name = xstrdup(path);

	if ((toks = tokenize(m)) == NULL)
		err(1, "tokenize");
	if (!dictionary_add_words(dict, toks, *len - 1))
		err(1, "dictionary_add_words");
	freetoks(toks);
	munmap(m, end);
	close(fd);
	return 1;
}

int
idx_files(struct dictionary *dict, struct db_entry **entries, size_t *len,
    int argc, char **argv)
{
	char *line = NULL;
	size_t linesize = 0, cap = *len;
	ssize_t linelen;
	int r = 0;

	if (argc > 0) {
		while (*argv) {
			if (!pfile(dict, entries, len, &cap, *argv))
				r = 1;
			argv++;
		}
		return r;
	}

	while ((linelen = getline(&line, &linesize, stdin)) != -1) {
		if (linelen > 1 && line[linelen-1] == '\n')
			line[linelen-1] = '\0';

		if (!pfile(dict, entries, len, &cap, line))
			r = 1;
	}

	free(line);
	if (ferror(stdin))
		err(1, "getline");
	return r;
}
