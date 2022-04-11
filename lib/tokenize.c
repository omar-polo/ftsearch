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

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "tokenize.h"

#ifndef WDELIMS
/* everything but a-zA-Z */
#define WDELIMS " \t\n!\"#$%&'()*+,-./0123456789:;<=>?@[\\]^_`{|}~"
#endif

char **
tokenize(const char *s)
{
	char *d, *dup, *t, **tok = NULL;
	void *newtok;
	size_t cap = 0, len = 0, newcap;

	if ((dup = strdup(s)) == NULL)
		return NULL;
	d = dup;

	for (t = d; *t; ++t)
		*t = tolower(*t);

	while ((t = strsep(&d, WDELIMS)) != NULL) {
		if (*t == '\0')
			continue;

		/* keep the space for a NULL terminator */
		if (len+1 >= cap) {
			newcap = cap * 1.5;
			if (newcap == 0)
				newcap = 8;
			newtok = recallocarray(tok, cap, newcap,
			    sizeof(char *));
			if (newtok == NULL)
				goto err;
			tok = newtok;
			cap = newcap;
		}

		if ((tok[len++] = strdup(t)) == NULL)
			goto err;
	}

	free(dup);
	return tok;

err:
	freetoks(tok);
	free(dup);
	return NULL;
}

void
freetoks(char **tok)
{
	char **i;

	if (tok == NULL)
		return;

	for (i = tok; *i != NULL; ++i)
		free(*i);
	free(tok);
}
