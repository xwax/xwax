/*
 * Copyright (C) 2024 Mark Hills <mark@xwax.org>
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

#ifndef SELECTOR_H
#define SELECTOR_H

#include <stdbool.h>

#include "library.h"
#include "listbox.h"
#include "index.h"

struct selector {
    struct library *library;
    struct index
        *view_index, /* base_index + search filter applied */
        *swap_index, /* used to swap between a and b indexes */
        index_a, index_b;

    struct listbox records, crates;
    bool toggled;
    int toggle_back, sort;
    struct record *target;
    struct observer on_activity, on_refresh, on_addition;

    size_t search_len;
    char search[256]; /* not unicode, one byte is one character */
    struct match match; /* the compiled search, kept in-sync */

    struct event changed;
};

void selector_init(struct selector *sel, struct library *lib);
void selector_clear(struct selector *sel);

void selector_set_lines(struct selector *sel, unsigned int lines);

void selector_up(struct selector *sel);
void selector_down(struct selector *sel);
void selector_page_up(struct selector *sel);
void selector_page_down(struct selector *sel);
void selector_top(struct selector *sel);
void selector_bottom(struct selector *sel);

struct record* selector_current(struct selector *sel);

void selector_prev(struct selector *sel);
void selector_next(struct selector *sel);
void selector_toggle(struct selector *sel);
void selector_toggle_order(struct selector *sel);
void selector_rescan(struct selector *sel);

void selector_search_expand(struct selector *sel);
void selector_search_refine(struct selector *sel, char key);

#endif
