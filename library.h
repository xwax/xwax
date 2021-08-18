/*
 * Copyright (C) 2021 Mark Hills <mark@xwax.org>
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

#ifndef LIBRARY_H
#define LIBRARY_H

#include <stdbool.h>
#include <stddef.h>

#include "index.h"
#include "observer.h"

/* A set of records, with several optimised indexes */

struct listing {
    struct index by_artist, by_bpm, by_order;
    struct event addition;
};

/* A single crate of records */

struct crate {
    bool is_fixed, is_busy;
    char *name;
    struct listing *listing;
    struct observer on_addition, on_completion;
    struct event activity, /* at the crate level, not the listing */
        refresh, addition;

    /* Optionally, the corresponding source */
    const char *scan, *path;
    struct excrate *excrate;
};

/* The complete music library, which consists of multiple crates */

struct library {
    struct listing storage; /* owns the record pointers */
    struct crate all, **crate;
    size_t crates;
};

int library_global_init(void);
void library_global_clear(void);

void listing_init(struct listing *l);
void listing_clear(struct listing *l);
struct record* listing_add(struct listing *l, struct record *r);

int library_init(struct library *li);
void library_clear(struct library *li);

struct record* get_record(char *line);

int library_import(struct library *lib, const char *scan, const char *path);
int library_rescan(struct library *l, struct crate *c);

#endif
