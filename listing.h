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

#ifndef LISTING_H
#define LISTING_H

#include <stddef.h>

#define SORT_ARTIST   0
#define SORT_BPM      1
#define SORT_PLAYLIST 2
#define SORT_END      3

struct record {
    char *pathname, *artist, *title;
    double bpm; /* or 0.0 if not known */
};

/* Listing points to records, but does not manage those pointers */

struct listing {
    struct record **record;
    size_t size, entries;
};

void listing_init(struct listing *ls);
void listing_clear(struct listing *ls);
void listing_blank(struct listing *ls);
int listing_add(struct listing *li, struct record *lr);
int listing_copy(const struct listing *src, struct listing *dest);
int listing_match(struct listing *src, struct listing *dest,
		  const char *match);
struct record* listing_insert(struct listing *ls, struct record *item,
                              int sort);
size_t listing_find(struct listing *ls, struct record *item, int sort);
void listing_debug(struct listing *ls);

#endif
