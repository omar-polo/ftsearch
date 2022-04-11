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

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "dictionary.h"

int
dictionary_init(struct dictionary *dict)
{
	memset(dict, 0, sizeof(*dict));
	return 1;
}

static inline int
add_docid(struct dict_entry *e, int docid)
{
	void *t;
	size_t newcap;

	if (e->len > 0 && e->ids[e->len-1] == docid)
		return 1;

	if (e->len == e->cap) {
		newcap = e->cap * 1.5;
		if (newcap == 0)
			newcap = 8;
		t = recallocarray(e->ids, e->cap, newcap, sizeof(*e->ids));
		if (t == NULL)
			return 0;
		e->ids = t;
		e->cap = newcap;
	}

	e->ids[e->len++] = docid;
	return 1;
}

int
dictionary_add(struct dictionary *dict, const char *word, int docid)
{
	struct dict_entry *e = NULL;
	void *newentr;
	size_t newcap, mid = 0, left = 0, right = dict->len;
	int r = 0;

	while (left < right) {
		mid = (left + right) / 2;
		e = &dict->entries[mid];
		r = strcmp(word, e->word);
		if (r < 0)
			right = mid;
		else if (r > 0)
			left = mid + 1;
		else
			return add_docid(e, docid);
	}

	if (r > 0)
		mid++;

	if (dict->len == dict->cap) {
		newcap = dict->cap * 1.5;
		if (newcap == 0)
			newcap = 8;
		newentr = recallocarray(dict->entries, dict->cap, newcap,
		    sizeof(*dict->entries));
		if (newentr == NULL)
			return 0;
		dict->entries = newentr;
		dict->cap = newcap;
	}

	e = &dict->entries[mid];
	if (e != dict->entries + dict->len) {
		size_t i = e - dict->entries;
		memmove(e+1, e, sizeof(*e) * (dict->len - i));
	}

	dict->len++;
	memset(e, 0, sizeof(*e));
	if ((e->word = strdup(word)) == NULL)
		return 0;
	return add_docid(e, docid);
}

int
dictionary_add_words(struct dictionary *dict, char **words, int docid)
{
	for (; *words != NULL; ++words) {
		if (!dictionary_add(dict, *words, docid))
			return 0;
	}

	return 1;
}

void
dictionary_free(struct dictionary *dict)
{
	size_t i;

	for (i = 0; i < dict->len; ++i) {
		free(dict->entries[i].word);
		free(dict->entries[i].ids);
	}

	free(dict->entries);
}
