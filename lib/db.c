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

#include <sys/mman.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "db.h"
#include "dictionary.h"

#define IDX_ENTRY_SIZE (DB_WORDLEN + sizeof(int64_t))

static int
write_dictionary(FILE *fp, struct dictionary *dict)
{
	off_t start;
	uint64_t pos;
	uint32_t n;
	size_t i, len;

	if ((uint64_t)dict->len > UINT32_MAX)
		return -1;

	n = dict->len;
	if (fwrite(&n, sizeof(n), 1, fp) != 1)
		return -1;

	if ((start = ftello(fp)) == -1)
		return -1;

	len = DB_WORDLEN + sizeof(int64_t);
	pos = start + (n * len);
	for (i = 0; i < dict->len; ++i) {
		char word[DB_WORDLEN];

		memset(word, 0, sizeof(word));
		strlcpy(word, dict->entries[i].word, sizeof(word));
		if (fwrite(word, sizeof(word), 1, fp) != 1)
			return -1;

		if (fwrite(&pos, sizeof(pos), 1, fp) != 1)
			return -1;

		/* one for the len */
		pos += sizeof(uint32_t) * (dict->entries[i].len + 1);
	}

	for (i = 0; i < dict->len; ++i) {
		size_t j;
		uint32_t t, x;

		x = dict->entries[i].len;
		if (fwrite(&x, sizeof(x), 1, fp) != 1)
			return -1;

		for (j = 0; j < x; ++j) {
			t = dict->entries[i].ids[j];
			if (fwrite(&t, sizeof(t), 1, fp) != 1)
				return -1;
		}
	}

	return 0;
}

int
db_create(FILE *fp, struct dictionary *dict, struct db_entry *entries,
    size_t n)
{
	int64_t endidx;
	size_t i;
	uint32_t version = DB_VERSION;

	if (n > INT32_MAX)
		return -1;

	if (fwrite(&version, sizeof(version), 1, fp) != 1)
		return -1;

	/* reserve space for the start pointer -- filled later */
	if (fseek(fp, sizeof(int64_t), SEEK_CUR) == -1)
		return -1;

	if (write_dictionary(fp, dict) == -1)
		return -1;

	if ((endidx = ftello(fp)) == -1)
		return -1;

	for (i = 0; i < n; ++i) {
		uint16_t namelen, descrlen = 0;

		namelen = strlen(entries[i].name);
		if (entries[i].descr != NULL)
			descrlen = strlen(entries[i].descr);

		if (fwrite(&namelen, sizeof(namelen), 1, fp) != 1)
			return -1;
		if (fwrite(entries[i].name, namelen+1, 1, fp) != 1)
			return -1;

		if (fwrite(&descrlen, sizeof(descrlen), 1, fp) != 1)
			return -1;
		if (descrlen > 0 &&
		    fwrite(entries[i].descr, descrlen, 1, fp) != 1)
			return -1;
		if (fwrite("", 1, 1, fp) != 1)
			return -1;
	}

	if (fseek(fp, sizeof(version), SEEK_SET) == -1)
		return -1;

	if (fwrite(&endidx, sizeof(endidx), 1, fp) != 1)
		return -1;

	return 0;
}

static int
initdb(struct db *db)
{
	off_t hdrlen = sizeof(uint32_t) + sizeof(int64_t) + sizeof(uint32_t);
	int64_t end_off;
	uint8_t *p = db->m;

	if (hdrlen > db->len)
		return -1;

	memcpy(&db->version, p, sizeof(db->version));
	p += sizeof(db->version);

	memcpy(&end_off, p, sizeof(end_off));
	p += sizeof(end_off);

	memcpy(&db->nwords, p, sizeof(db->nwords));
	p += sizeof(db->nwords);

	db->idx_start = p;
	db->idx_end = p + db->nwords * IDX_ENTRY_SIZE;
	db->list_start = db->idx_end;
	db->list_end = db->m + end_off;
	db->docs_start = db->list_end;
	db->docs_end = db->m + db->len;

	if (db->idx_end > db->docs_end)
		return -1;
	if (db->list_end > db->docs_end)
		return -1;

	return 0;
}

int
db_open(struct db *db, int fd)
{
	memset(db, 0, sizeof(*db));

	if ((db->len = lseek(fd, 0, SEEK_END)) == -1)
		return -1;

	if (lseek(fd, 0, SEEK_SET) == -1)
		return -1;

	db->m = mmap(NULL, db->len, PROT_READ, MAP_PRIVATE, fd, 0);
	if (db->m == MAP_FAILED)
		return -1;

	if (initdb(db) == -1) {
		db_close(db);
		return -1;
	}

	return 0;
}

static int
db_countdocs(struct db *db, struct db_entry *e, void *d)
{
	struct db_stats *stats = d;

	stats->ndocs++;
	return 0;
}

static int
db_idx_compar(const void *key, const void *elem)
{
	const char *word = key;
	const char *idx_entry = elem;

	if (idx_entry[DB_WORDLEN-1] != '\0')
		return -1;
	return strcmp(word, idx_entry);
}

static inline uint32_t *
db_getdocs(struct db *db, const uint8_t *entry, size_t *len)
{
	int64_t pos;
	uint32_t l;

	entry += DB_WORDLEN;
	memcpy(&pos, entry, sizeof(pos));

	entry = db->m + pos;
	if (entry < db->list_start || entry > db->list_end)
		return NULL;

	memcpy(&l, entry, sizeof(l));
	entry += sizeof(l);
	*len = l;
	return (uint32_t *)entry;
}

uint32_t *
db_word_docs(struct db *db, const char *word, size_t *len)
{
	uint8_t *e;

	*len = 0;

	e = bsearch(word, db->idx_start, db->nwords, IDX_ENTRY_SIZE,
	    db_idx_compar);
	if (e == NULL)
		return NULL;
	return db_getdocs(db, e, len);
}

int
db_stats(struct db *db, struct db_stats *stats)
{
	const uint8_t *p;
	size_t l, maxl = 0, idlen;

	memset(stats, 0, sizeof(*stats));

	if (db_listall(db, db_countdocs, stats) == -1)
		return -1;

	stats->nwords = db->nwords;

	p = db->idx_start;
	while (p < db->idx_end) {
		if (p + DB_WORDLEN > db->idx_end)
			return -1;

		if (p[DB_WORDLEN-1] != '\0')
			return -1;

		l = strlen(p);
		if (l > maxl) {
			maxl = l;
			stats->longest_word = p;
		}

		if (db_getdocs(db, p, &idlen) == NULL)
			return -1;

		if (idlen > stats->most_popular_ndocs) {
			stats->most_popular_ndocs = idlen;
			stats->most_popular = p;
		}

		p += IDX_ENTRY_SIZE;
	}

	return 0;
}

static inline uint8_t *
db_extract_doc(struct db *db, uint8_t *p, struct db_entry *e)
{
	uint16_t namelen, descrlen;

	/*
	 * namelen[2] name[namelen]
	 * descrlen[2] descr[descrlen]
	 */

	if (p + 2 > db->docs_end)
		return NULL;
	memcpy(&namelen, p, sizeof(namelen));
	p += sizeof(namelen);

	if (p + namelen > db->docs_end || p[namelen] != '\0')
		return NULL;
	e->name = p;
	p += namelen + 1;

	if (p + 2 > db->docs_end)
		return NULL;
	memcpy(&descrlen, p, sizeof(descrlen));
	p += sizeof(descrlen);

	if (p + descrlen > db->docs_end || p[descrlen] != '\0')
		return NULL;
	e->descr = p;
	p += descrlen + 1;

	return p;
}

int
db_listall(struct db *db, db_hit_cb cb, void *data)
{
	uint8_t *p = db->docs_start;

	while (p < db->docs_end) {
		struct db_entry e;

		if ((p = db_extract_doc(db, p, &e)) == NULL)
			return -1;

		if (cb(db, &e, data) == -1)
			return -1;
	}

	return 0;
}

int
db_doc_by_id(struct db *db, int docid, struct db_entry *e)
{
	uint8_t *p = db->docs_start;
	int n = 0;

	while (p < db->docs_end) {
		if ((p = db_extract_doc(db, p, e)) == NULL)
			return -1;

		if (n == docid)
			return 0;

		n++;
	}

	return -1;
}

void
db_close(struct db *db)
{
	munmap(db->m, db->len);
	memset(db, 0, sizeof(*db));
}
