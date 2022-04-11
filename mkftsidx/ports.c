/*
 * Copyright (c) 2022 Omar Polo <op@openbsd.org>
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
#include <stdio.h>
#include <stdlib.h>

#include <sqlite3.h>

#include "db.h"
#include "dictionary.h"
#include "tokenize.h"

#include "mkftsidx.h"

#ifndef SQLPORTS
#define SQLPORTS "/usr/local/share/sqlports"
#endif

#define QNUM "select count(*) from portsq;"
#define QALL "select pkgstem, comment, descr_contents from portsq;"

static int
countports(sqlite3 *db)
{
	sqlite3_stmt *stmt;
	int r, n = -1;

	r = sqlite3_prepare_v2(db, QNUM, -1, &stmt, NULL);
	if (r != SQLITE_OK) {
		warnx("failed to prepare statement: %s",
		    sqlite3_errstr(r));
		return -1;
	}

	r = sqlite3_step(stmt);
	if (r == SQLITE_ROW)
		n = sqlite3_column_int(stmt, 0);

	sqlite3_finalize(stmt);
	return n;
}

int
idx_ports(struct dictionary *dict, struct db_entry **entries, size_t *len,
    int argc, char **argv)
{
	const char *dbpath;
	sqlite3 *db;
	sqlite3_stmt *stmt;
	size_t i;
	int r;

	if (argc > 1)
		usage();
	else if (argc == 1)
		dbpath = *argv;
	else
		dbpath = SQLPORTS;

	if ((r = sqlite3_open(dbpath, &db)) != SQLITE_OK)
		errx(1, "can't open %s: %s", dbpath, sqlite3_errstr(r));

	if ((r = countports(db)) == -1 || r == 0) {
		warnx("error querying the db or empty portsq table!");
		goto done;
	}
	*len = r;

	if ((*entries = calloc(*len, sizeof(**entries))) == NULL)
		err(1, "calloc");

	r = sqlite3_prepare_v2(db, QALL, -1, &stmt, NULL);
	if (r != SQLITE_OK)
		errx(1, "failed to prepare statement: %s", sqlite3_errstr(r));

	for (i = 0; i < *len; ++i) {
		const char *pkgstem, *comment, *descr;
		char *doc, **toks;

		r = sqlite3_step(stmt);
		if (r == SQLITE_DONE)
			break;
		if (r != SQLITE_ROW)
			errx(1, "sqlite3_step: %s", sqlite3_errstr(r));

		pkgstem = sqlite3_column_text(stmt, 0);
		comment = sqlite3_column_text(stmt, 1);
		descr = sqlite3_column_text(stmt, 2);

		(*entries)[i].name = xstrdup(pkgstem);
		(*entries)[i].descr = xstrdup(comment);

		r = asprintf(&doc, "%s %s %s", pkgstem,
		    comment != NULL ? comment : "",
		    descr != NULL ? descr : "");
		if (r == -1)
			err(1, "asprintf");

		if ((toks = tokenize(doc)) == NULL)
			err(1, "tokenize");
		if (!dictionary_add_words(dict, toks, i))
			err(1, "dictionary_add_words");
		freetoks(toks);
		free(doc);
	}

done:
	sqlite3_close(db);
	return 0;
}
