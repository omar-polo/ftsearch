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

#define DB_VERSION	 0
#define DB_WORDLEN	32

struct db {
	uint8_t	*m;
	off_t	 len;
	uint32_t version;
	uint32_t nwords;

	uint8_t	*idx_start;
	uint8_t	*idx_end;
	uint8_t	*list_start;
	uint8_t	*list_end;
	uint8_t	*docs_start;
	uint8_t	*docs_end;
};

struct db_stats {
	size_t		 nwords;
	size_t		 ndocs;
	const char	*longest_word;
	const char	*most_popular;
	size_t		 most_popular_ndocs;
};

struct db_entry {
	char	*name;
	char	*descr;
};

typedef int (*db_hit_cb)(struct db *, struct db_entry *, void *);

struct dictionary;

int		 db_create(FILE *, struct dictionary *, struct db_entry *, size_t);
int		 db_open(struct db *, int);
uint32_t	*db_word_docs(struct db *, const char *, size_t *);
int		 db_stats(struct db *, struct db_stats *);
int		 db_listall(struct db *, db_hit_cb, void *);
int		 db_doc_by_id(struct db *, int, struct db_entry *);
void		 db_close(struct db *);
