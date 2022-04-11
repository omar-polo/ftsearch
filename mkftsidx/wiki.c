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
#include <expat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "dictionary.h"
#include "tokenize.h"

#include "mkftsidx.h"

enum {
	N_UNK,
	N_TIT,
	N_URL,
	N_ABS,
};

struct mydata {
	struct dictionary	*dict;
	struct db_entry		*entries;
	size_t			 len;
	size_t			 cap;

	int next;
	char *title;
	char *url;
	char *abstract;
};

static void
el_start(void *data, const char *element, const char **attr)
{
	struct mydata *d = data;

	if (!strcmp(element, "title")) {
		d->next = N_TIT;
	} else if (!strcmp(element, "url")) {
		d->next = N_URL;
	} else if (!strcmp(element, "abstract")) {
		d->next = N_ABS;
	}
}

static void
append_text(char **text, const char *s, int len)
{
	char *t, *out, *orig;

	if ((t = calloc(1, len + 1)) == NULL)
		err(1, "calloc");
	memcpy(t, s, len);

	if ((orig = *text) == NULL)
		orig = "";
	if (asprintf(&out, "%s%s", orig, t) == -1)
		err(1, "asprintf");
	free(*text);
	*text = out;
	free(t);
}

static void
on_text(void *data, const char *s, int len)
{
	struct mydata *d = data;

	switch (d->next) {
	case N_TIT:
		append_text(&d->title, s, len);
		break;
	case N_URL:
		append_text(&d->url, s, len);
		break;
	case N_ABS:
		append_text(&d->abstract, s, len);
		break;
	default:
		break;
	}
}

static void
el_end(void *data, const char *element)
{
	struct mydata *d = data;
	struct db_entry *e;
	size_t newcap;
	const char *title;
	char *doc, **toks;
	void *t;
	int r, next;

	next = d->next;
	d->next = N_UNK;
	if ((next == N_TIT && !strcmp(element, "title")) ||
	    (next == N_URL && !strcmp(element, "url")) ||
	    (next == N_ABS && !strcmp(element, "abstract")) ||
	    strcmp(element, "doc"))
		return;

	if (d->len == d->cap) {
		newcap = d->cap * 1.5;
		if (newcap == 0)
			newcap = 8;
		t = recallocarray(d->entries, d->cap, newcap,
		    sizeof(*d->entries));
		if (t == NULL)
			err(1, "recallocarray");
		d->entries = t;
		d->cap = newcap;
	}

	title = d->title;
	if (!strncmp(title, "Wikipedia: ", 11))
		title += 11;

	e = &d->entries[d->len++];
	e->name = xstrdup(d->url);
	e->descr = xstrdup(title);

	if (d->len % 1000 == 0)
		printf("=> %zu\n", d->len);

	r = asprintf(&doc, "%s %s", title, d->abstract);
	if (r == -1)
		err(1, "asprintf");

	if ((toks = tokenize(doc)) != NULL) {
		if (!dictionary_add_words(d->dict, toks, d->len-1))
			err(1, "dictionary_add_words");
		freetoks(toks);
	}
	free(doc);

	free(d->title);
	free(d->url);
	free(d->abstract);

	d->title = NULL;
	d->url = NULL;
	d->abstract = NULL;
}

int
idx_wiki(struct dictionary *dict, struct db_entry **entries, size_t *len,
    int argc, char **argv)
{
	struct mydata d;
	XML_Parser parser;
	const char *xmlpath;
	char buf[BUFSIZ];
	int done = 0;
	FILE *fp;
	size_t r;

	if (argc != 1) {
		warnx("missing path to xml file");
		usage();
	}
	xmlpath = *argv;

	memset(&d, 0, sizeof(d));
	d.dict = dict;

	if ((parser = XML_ParserCreate(NULL)) == NULL)
		err(1, "XML_ParserCreate");
	XML_SetUserData(parser, &d);
	XML_SetElementHandler(parser, el_start, el_end);
	XML_SetCharacterDataHandler(parser, on_text);

	if ((fp = fopen(xmlpath, "r")) == NULL)
		err(1, "can't open %s", xmlpath);

	do {
		r = fread(buf, 1, sizeof(buf), fp);
		done = r != sizeof(buf);
		if (!XML_Parse(parser, buf, r, done))
			errx(1, "can't parse: %s at %s:%lu",
			    XML_ErrorString(XML_GetErrorCode(parser)),
			    xmlpath,
			    XML_GetCurrentLineNumber(parser));
	} while (!done);

	fclose(fp);
	XML_ParserFree(parser);

	*len = d.len;
	*entries = d.entries;

	return 0;
}
