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

#ifndef LIBRARY_H
#define LIBRARY_H

#include <stdbool.h>

#include "listing.h"

#define CRATE_ALL "All records"

struct crate_t {
    bool is_fixed;
    char *name;
    struct listing_t listing;
};

struct library_t {
    struct listing_t storage;
    struct crate_t **crate;
    int crates;
};

int library_init(struct library_t *li);
void library_clear(struct library_t *li);
struct crate_t* library_new_crate(struct library_t *lib, char *name,
                                  bool is_fixed);
struct crate_t* library_get_crate(struct library_t *lib, char *name);
int library_import(struct library_t *lib, const char *scan, const char *path);

#endif
