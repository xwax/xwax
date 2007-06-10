/*
 * Copyright (C) 2007 Mark Hills <mark@pogo.org.uk>
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

#ifndef LIBRARY_H
#define LIBRARY_H

#define MAX_ARTIST 128
#define MAX_TITLE 128
#define MAX_NAME 512
#define MAX_PATHNAME 1024

struct record_t {
    char artist[MAX_ARTIST],
        title[MAX_TITLE],
        name[MAX_NAME],
        pathname[MAX_PATHNAME];
};

/* Library acts as a storage of the actual strings */

struct library_t {
    struct record_t *record;
    int size, entries;
};

/* Listing just points within a library */

struct listing_t {
    struct record_t **record;
    int size, entries;
};

int library_init(struct library_t *li);
int library_clear(struct library_t *li);
int library_add(struct library_t *li, struct record_t *lr);
int library_import(struct library_t *li, char *path);
int library_get_listing(struct library_t *li, struct listing_t *ls);

int listing_init(struct listing_t *ls);
int listing_clear(struct listing_t *ls);
int listing_blank(struct listing_t *ls);
int listing_add(struct listing_t *li, struct record_t *lr);
int listing_match(struct listing_t *src, struct listing_t *dest, char *match);
int listing_debug(struct listing_t *ls);
int listing_sort(struct listing_t *ls);

#endif
