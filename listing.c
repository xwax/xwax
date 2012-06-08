/*
 * Copyright (C) 2012 Mark Hills <mark@pogo.org.uk>
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

#define BLOCK 1024
#define MAX_WORDS 32
#define SEPARATOR ' '

/*
 * Initialise a record listing
 */

void listing_init(struct listing *ls)
{
    ls->record = NULL;
    ls->size = 0;
    ls->entries = 0;
}

/*
 * Deallocate resources associated with this listing
 *
 * The listing does not allocate records itself, so it is not
 * responsible for deallocating them.
 */

void listing_clear(struct listing *ls)
{
    if (ls->record != NULL)
        free(ls->record);
}

/*
 * Blank the listing so it contains no entries
 *
 * We don't de-allocate memory, but this gives us an advantage where
 * listing re-use is of similar size.
 */

void listing_blank(struct listing *ls)
{
    ls->entries = 0;
}

/*
 * Enlarge the storage space of the listing to at least the target
 * size
 *
 * Return: 0 on success or -1 on memory allocation failure
 * Post: size of listing is greater than or equal to target
 */

static int enlarge(struct listing *ls, size_t target)
{
    size_t p;
    struct record **ln;

    if (target <= ls->size)
        return 0;

    p = target + BLOCK - 1; /* pre-allocate additional entries */

    ln = realloc(ls->record, sizeof(struct record*) * p);
    if (ln == NULL) {
        perror("realloc");
        return -1;
    }

    ls->record = ln;
    ls->size = p;
    return 0;
}

/*
 * Add a record to the listing
 *
 * Return: 0 on success or -1 on memory allocation failure
 */

int listing_add(struct listing *ls, struct record *lr)
{
    if (enlarge(ls, ls->entries + 1) == -1)
        return -1;

    ls->record[ls->entries++] = lr;
    return 0;
}

/*
 * Standard comparison function between two records
 */

static int record_cmp_artist(const struct record *a, const struct record *b)
{
    int r;

    r = strcasecmp(a->artist, b->artist);
    if (r < 0)
        return -1;
    else if (r > 0)
        return 1;

    r = strcasecmp(a->title, b->title);
    if (r < 0)
        return -1;
    else if (r > 0)
        return 1;

    return strcmp(a->pathname, b->pathname);
}

/*
 * Compare two records principally by BPM, fastest to slowest
 * followed by unknown
 */

static int record_cmp_bpm(const struct record *a, const struct record *b)
{
    if (a->bpm < b->bpm)
        return 1;

    if (a->bpm > b->bpm)
        return -1;

    return record_cmp_artist(a, b);
}

/*
 * Check if a record matches the given string. This function is the
 * definitive code which defines what constitutes a 'match'.
 *
 * Return: true if this is a match, otherwise false
 */

static bool record_match(struct record *re, const char *match)
{
    if (strcasestr(re->artist, match) != NULL)
        return true;
    if (strcasestr(re->title, match) != NULL)
        return true;
    return false;
}

/*
 * Check for a match against all the strings in a given
 * NULL-terminated array
 *
 * Return: true if the given record matches, otherwise false
 */

static bool record_match_all(struct record *re, char **matches)
{
    while (*matches != NULL) {
        if (!record_match(re, *matches))
            return false;
        matches++;
    }
    return true;
}

/*
 * Copy the source listing
 *
 * Return: 0 on success or -1 on memory allocation failure
 * Post: on failure, dest is valid but incomplete
 */

int listing_copy(const struct listing *src, struct listing *dest)
{
    int n;

    listing_blank(dest);

    for (n = 0; n < src->entries; n++) {
	if (listing_add(dest, src->record[n]) != 0)
	    return -1;
    }

    return 0;
}

/*
 * Find entries from the source listing with match the given string
 *
 * Copy the subset of the source listing which matches the given
 * string into the destination. This function defines what constitutes
 * a match.
 *
 * Return: 0 on success, or -1 on memory allocation failure
 * Post: on failure, dest is valid but incomplete
 */

int listing_match(struct listing *src, struct listing *dest,
		  const char *match)
{
    int n;
    char *buf, *words[MAX_WORDS];
    struct record *re;

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

/*
 * Binary search of sorted listing
 *
 * We implement our own binary search rather than using the bsearch()
 * from stdlib.h, because we need to know the position to insert to if
 * the item is not found.
 *
 * Pre: base is sorted
 * Return: position of match >= item
 * Post: on exact match, *found is true
 */

static size_t bin_search(struct record **base, size_t n,
                         struct record *item, int sort,
                         bool *found)
{
    int r;
    size_t mid;
    struct record *x;

    /* Return the first entry ordered after this one */

    if (n == 0) {
        *found = false;
        return 0;
    }

    mid = n / 2;
    x = base[mid];

    switch (sort) {
    case SORT_ARTIST:
        r = record_cmp_artist(item, x);
        break;
    case SORT_BPM:
        r = record_cmp_bpm(item, x);
        break;
    case SORT_PLAYLIST:
    default:
        abort();
    }

    if (r < 0)
        return bin_search(base, mid, item, sort, found);
    if (r > 0) {
        return mid + 1
            + bin_search(base + mid + 1, n - mid - 1, item, sort, found);
    }

    *found = true;
    return mid;
}

/*
 * Insert or re-use an entry in a sorted listing
 *
 * Pre: listing is sorted
 * Return: pointer to item, or existing entry; NULL if out of memory
 * Post: listing is sorted and contains item or a matching item
 */

struct record* listing_insert(struct listing *ls, struct record *item,
                              int sort)
{
    bool found;
    size_t z;

    z = bin_search(ls->record, ls->entries, item, sort, &found);
    if (found)
        return ls->record[z];

    /* Insert the new item */

    if (enlarge(ls, ls->entries + 1) == -1)
        return NULL;

    memmove(ls->record + z + 1, ls->record + z,
            sizeof(struct record*) * (ls->entries - z));
    ls->record[z] = item;
    ls->entries++;

    return item;
}

/*
 * Find an identical entry, or the nearest match
 */

size_t listing_find(struct listing *ls, struct record *item, int sort)
{
    bool found;
    size_t z;

    z = bin_search(ls->record, ls->entries, item, sort, &found);
    return z;
}

/*
 * Debug the content of a listing to standard error
 */

void listing_debug(struct listing *ls)
{
    int n;

    for (n = 0; n < ls->entries; n++)
        fprintf(stderr, "%d: %s\n", n, ls->record[n]->pathname);
}
