/*
 * Copyright (C) 2010 Mark Hills <mark@pogo.org.uk>,
 *                    Yves Adler <yves.adler@googlemail.com>
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

#ifndef SELECTOR_H
#define SELECTOR_H

#include <stdbool.h>
#include <stddef.h>

#include "library.h"
#include "listing.h"

/* Managed context of a scrolling window, of a number of fixed-height
 * lines, backed by a list of a known number of entries */

struct scroll_t {
    int lines, offset, entries, selected;
};

struct selector_t {
    struct library_t *library;
    struct listing_t
        *view_listing, /* base_listing + search filter applied */
        *swap_listing, /* used to swap between a and b listings */
        listing_a, listing_b;

    struct scroll_t records, crates;
    bool toggled;
    int toggle_back;

    size_t search_len;
    char search[256];
};

void selector_init(struct selector_t *sel, struct library_t *lib);
void selector_clear(struct selector_t *sel);

void selector_set_lines(struct selector_t *sel, unsigned int lines);

void selector_up(struct selector_t *sel);
void selector_down(struct selector_t *sel);
void selector_page_up(struct selector_t *sel);
void selector_page_down(struct selector_t *sel);
void selector_top(struct selector_t *sel);
void selector_bottom(struct selector_t *sel);

struct record_t* selector_current(struct selector_t *sel);

void selector_prev(struct selector_t *sel);
void selector_next(struct selector_t *sel);
void selector_toggle(struct selector_t *sel);

void selector_search_expand(struct selector_t *sel);
void selector_search_refine(struct selector_t *sel, char key);

#endif
