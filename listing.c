/*
 * Copyright (C) 2010 Mark Hills <mark@pogo.org.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#define _GNU_SOURCE /* strcasestr(), strdupa() */
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "listing.h"

#define BLOCK 256
#define MAX_WORDS 32
#define SEPARATOR ' '


void listing_init(struct listing_t *ls)
{
    ls->record = NULL;
    ls->entries = 0;
}


void listing_clear(struct listing_t *ls)
{
    if (ls->record != NULL)
        free(ls->record);
}


void listing_blank(struct listing_t *ls)
{
    ls->entries = 0;
}


/* Add a record to the listing. Return 0 on success, or -1 on memory
 * allocation failure */

int listing_add(struct listing_t *ls, struct record_t *lr)
{
    struct record_t **ln;

    if (ls->record == NULL) { /* initial entry */

        ln = malloc(sizeof(struct record_t*) * BLOCK);
        if (ln == NULL) {
            perror("malloc");
            return -1;
        }

        ls->record = ln;
        ls->size = BLOCK;

    } else if (ls->entries == ls->size) {

        ln = realloc(ls->record, sizeof(struct record_t*) * ls->size * 2);
        if (ln == NULL) {
            perror("realloc");
            return -1;
        }

        ls->record = ln;
        ls->size *= 2;
    }

    ls->record[ls->entries++] = lr;

    return 0;
}


static int record_cmp(const struct record_t *a, const struct record_t *b)
{
    int r;

    r = strcmp(a->artist, b->artist);
    if (r < 0)
        return -1;
    else if (r > 0)
        return 1;

    r = strcmp(a->title, b->title);
    if (r < 0)
        return -1;
    else if (r > 0)
        return 1;

    return 0;
}


static void quicksort_swap(struct listing_t *ls, int i, int j)
{
    struct record_t *tmp;

    tmp = ls->record[i];
    ls->record[i] = ls->record[j];
    ls->record[j] = tmp;
}


static int quicksort_partition(struct listing_t *ls, int left, int right)
{
    struct record_t *pivot;

    pivot = ls->record[left];

    while (left < right) {
        while ((left < right) && (record_cmp(ls->record[right], pivot) >= 0))
            right--;
        quicksort_swap(ls, right, left);
        while ((left < right) && (record_cmp(pivot, ls->record[left]) >= 0))
            left++;
        quicksort_swap(ls, right, left);
    }

    return left;
}


static void quicksort(struct listing_t *ls, int left, int right)
{
    int pivot;

    if (left < right) {
        pivot = quicksort_partition(ls, left, right);
        quicksort(ls, left, pivot);
        quicksort(ls, pivot + 1, right);
    }
}


void listing_sort(struct listing_t *ls)
{
    quicksort(ls, 0, ls->entries - 1);
}


/* Return true if the given record matches the given string. This
 * function defines what constitutes a 'match' */

static bool record_match(struct record_t *re, const char *match)
{
    if (strcasestr(re->artist, match) != NULL)
        return true;
    if (strcasestr(re->title, match) != NULL)
        return true;
    return false;
}


/* Return true if the given record matches all of the given strings
 * in a NULL-terminated array */

static bool record_match_all(struct record_t *re, char **matches)
{
    while (*matches != NULL) {
        if (!record_match(re, *matches))
            return false;
        matches++;
    }
    return true;
}


/* Copy the source listing; return 0 on succes or -1 on memory
 * allocation failure, which leaves dest valid but incomplete */

int listing_copy(const struct listing_t *src, struct listing_t *dest)
{
    int n;

    listing_blank(dest);

    for (n = 0; n < src->entries; n++) {
	if (listing_add(dest, src->record[n]) != 0)
	    return -1;
    }

    return 0;
}


/* Copy the subset of the source listing which matches the given
 * string; this function defines what constitutes a match; return 0 on
 * success, or -1 on memory allocation failure, which leaves dest
 * valid but incomplete */

int listing_match(struct listing_t *src, struct listing_t *dest,
		  const char *match)
{
    int n;
    char *buf, *words[MAX_WORDS];
    struct record_t *re;

    fprintf(stderr, "Matching '%s'\n", match);

    buf = strdupa(match);
    n = 0;
    for (;;) {
        char *s;

        if (n == MAX_WORDS - 1) {
            fputs("Ignoring excessive words in match string.\n", stderr);
            break;
        }

        words[n] = buf;
        n++;

        s = strchr(buf, SEPARATOR);
        if (s == NULL)
            break;
        *s = '\0';
        buf = s + 1; /* skip separator */
    }
    words[n] = NULL; /* terminate list */

    listing_blank(dest);

    for (n = 0; n < src->entries; n++) {
        re = src->record[n];

        if (record_match_all(re, words)) {
            if (listing_add(dest, re) == -1)
                return -1;
        }
    }

    return 0;
}


void listing_debug(struct listing_t *ls)
{
    int n;

    for (n = 0; n < ls->entries; n++)
        fprintf(stderr, "%d: %s\n", n, ls->record[n]->pathname);
}
