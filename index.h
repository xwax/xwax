/*
 * Copyright (C) 2025 Mark Hills <mark@xwax.org>
 *
 * This file is part of "xwax".
 *
 * "xwax" is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * "xwax" is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef INDEX_H
#define INDEX_H

#include <stddef.h>

#define SORT_ARTIST   0
#define SORT_BPM      1
#define SORT_PLAYLIST 2
#define SORT_END      3

/* A single music track in our listings */

struct record {
    char *pathname, *artist, *title; /* a single malloc */

    /* An optional extra string may be used to match against search
     * input; allows us to handle locale but still type in ASCII */

    char *match; /* or NULL */

    double bpm; /* or 0.0 if not known */
};

/* Index points to records, but does not manage those pointers */

struct index {
    struct record **record;
    size_t size, entries;
};

/* A 'compiled' search criteria, so we can repeat searches and
 * matches efficiently */

struct match {
    char buf[512];
    char *words[32]; /* NULL-terminated array */
};

void index_init(struct index *ls);
void index_clear(struct index *ls);
void index_blank(struct index *ls);
void index_add(struct index *li, struct record *lr);
bool record_match(struct record *re, const struct match *h);
int index_copy(const struct index *src, struct index *dest);
void match_compile(struct match *h, const char *d);
int index_match(struct index *src, struct index *dest,
                const struct match *match);
struct record* index_insert(struct index *ls, struct record *item,
                            int sort);
int index_reserve(struct index *i, unsigned int n);
size_t index_find(struct index *ls, struct record *item, int sort);
void index_debug(struct index *ls);

#endif
