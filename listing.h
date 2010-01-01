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

#ifndef LISTING_H
#define LISTING_H

struct record_t {
    char *pathname, *artist, *title;
};

/* Listing points to records, but does not manage those pointers */

struct listing_t {
    struct record_t **record;
    int size, entries;
};

void listing_init(struct listing_t *ls);
void listing_clear(struct listing_t *ls);
void listing_blank(struct listing_t *ls);
int listing_add(struct listing_t *li, struct record_t *lr);
int listing_copy(const struct listing_t *src, struct listing_t *dest);
int listing_match(struct listing_t *src, struct listing_t *dest,
		  const char *match);
void listing_debug(struct listing_t *ls);
void listing_sort(struct listing_t *ls);

#endif
