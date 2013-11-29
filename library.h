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

/* A single crate of records */

struct crate {
    bool is_fixed;
    char *name;
    struct index by_artist, by_bpm, by_order;
};

/* The complete music library, which consists of multiple crates */

struct library {
    struct crate all, **crate;
    size_t crates;
};

int library_init(struct library *li);
void library_clear(struct library *li);

int library_import(struct library *lib, const char *scan, const char *path);

#endif
