/*
 * Copyright (C) 2013 Mark Hills <mark@xwax.org>
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

#include <stdbool.h>
#include <stddef.h>

#include "index.h"
#include "observer.h"

/* A list of records, with several optimised indexes */

struct listing {
    struct index by_artist, by_bpm, by_order;
};

/* A single crate of records */

struct crate {
    bool is_fixed;
    char *name;
    struct listing listing;
    struct event addition;
};

/* The complete music library, which consists of multiple crates */

struct library {
    struct crate all, **crate;
    size_t crates;
};

void listing_init(struct listing *l);
void listing_clear(struct listing *l);
struct record* listing_add(struct listing *l, struct record *r);
struct record* crate_add(struct crate *c, struct record *r);

int library_init(struct library *li);
void library_clear(struct library *li);

struct record* get_record(char *line);

int library_import(struct library *lib, const char *scan, const char *path);

#endif
