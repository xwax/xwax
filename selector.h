/*
 * Copyright (C) 2009 Mark Hills <mark@pogo.org.uk>,
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

#include "library.h"
#include "listing.h"

struct selector_t {
    struct listing_t *base_listing, /* (unchanged) listing from interface_t */
        *view_listing, /* base_listing + search filter applied */
        *swap_listing, /* used to swap between a and b listings */
        listing_a, listing_b;

    int lst_lines, lst_selected, lst_offset,
        search_len;
    char search[256];
};

void selector_init(struct selector_t *sel, struct listing_t *listing);
void selector_clear(struct selector_t *sel);

void selector_lst_prev(struct selector_t *sel, int count);
void selector_lst_next(struct selector_t *sel, int count);
struct record_t* selector_lst_current(struct selector_t *sel);

void selector_search_expand(struct selector_t *sel);
void selector_search_refine(struct selector_t *sel, char key);

#endif
