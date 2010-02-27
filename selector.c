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

#include <assert.h>
#include <stdlib.h>

#include "selector.h"


static void scroll_reset(struct scroll_t *s)
{
    s->lines = 0;
    s->offset = 0;
    s->entries = 0;
    s->selected = -1;
}


/* Set the number of lines displayed on screen. The current selection
 * is moved to within range. */

static void scroll_set_lines(struct scroll_t *s, unsigned int lines)
{
    s->lines = lines;
    if (s->selected >= s->offset + s->lines)
        s->selected = s->offset + s->lines - 1;
    if (s->offset + s->lines > s->entries) {
        s->offset = s->entries - s->lines;
        if (s->offset < 0)
            s->offset = 0;  
    }
}


/* Set the number of entries in the list which backs the scrolling
 * display. Bring the current selection within the bounds given. */

static void scroll_set_entries(struct scroll_t *s, unsigned int entries)
{
    s->entries = entries;
    if (s->selected >= s->entries)
        s->selected = s->entries - 1;
    if (s->offset + s->lines > s->entries) {
        s->offset = s->entries - s->lines;
        if (s->offset < 0)
            s->offset = 0;
    }

    /* If we went previously had zero entries, reset the selection */

    if (s->selected < 0)
        s->selected = 0;
}


/* Scroll the selection up by n lines. Move the window offset if
 * needed */

static void scroll_up(struct scroll_t *s, unsigned int n)
{
    s->selected -= n;
    if (s->selected < 0)
        s->selected = 0;

    /* Move the viewing offset up, if necessary */

    if (s->selected < s->offset) {
        s->offset = s->selected - s->lines / 2 + 1;
        if (s->offset < 0)
            s->offset = 0;
    }
}


static void scroll_down(struct scroll_t *s, unsigned int n)
{
    s->selected += n;
    if (s->selected >= s->entries)
        s->selected = s->entries - 1;

    /* Move the viewing offset down, if necessary */

    if (s->selected >= s->offset + s->lines) {
        s->offset = s->selected - s->lines / 2;
        if (s->offset + s->lines > s->entries)
            s->offset = s->entries - s->lines;
    }
}


/* Scroll to the first entry on the list */

static void scroll_first(struct scroll_t *s)
{
    s->selected = 0;
    s->offset = 0;
}


/* Scroll to the final entry on the list */

static void scroll_last(struct scroll_t *s)
{
    s->selected = s->entries - 1;
    s->offset = s->selected - s->lines + 1;
    if (s->offset < 0)
        s->offset = 0;
}


/* Scroll to an entry by index */

static void scroll_to(struct scroll_t *s, unsigned int n)
{
    s->selected = n;

    /* Move the viewing offset down, if necessary */

    if (s->selected >= s->offset + s->lines) {
        s->offset = s->selected - s->lines / 2;
        if (s->offset + s->lines > s->entries)
            s->offset = s->entries - s->lines;
    }

    /* Move the viewing offset up, if necessary */

    if (s->selected < s->offset) {
        s->offset = s->selected - s->lines / 2 + 1;
        if (s->offset < 0)
            s->offset = 0;
    }
}


/* Return the index of the current selected list entry, or -1 if
 * no current selection */

static int scroll_current(struct scroll_t *s)
{
    if (s->entries == 0) {
        return -1;
    } else {
        return s->selected;
    }
}


/* Return the listing which acts as the starting point before
 * string matching, based on the current crate */

static struct listing_t* initial(struct selector_t *sel)
{
    return &sel->library->crate[sel->crates.selected]->listing;
}


void selector_init(struct selector_t *sel, struct library_t *lib)
{
    sel->library = lib;

    scroll_reset(&sel->records);
    scroll_reset(&sel->crates);

    scroll_set_entries(&sel->crates, lib->crates);

    sel->toggled = false;
    sel->search[0] = '\0';
    sel->search_len = 0;

    listing_init(&sel->listing_a);
    listing_init(&sel->listing_b);
    sel->view_listing = &sel->listing_a;
    sel->swap_listing = &sel->listing_b;

    (void)listing_copy(initial(sel), sel->view_listing);
    scroll_set_entries(&sel->records, sel->view_listing->entries);
}


void selector_clear(struct selector_t *sel)
{
    listing_clear(&sel->listing_a);
    listing_clear(&sel->listing_b);
}


void selector_set_lines(struct selector_t *sel, unsigned int lines)
{
    scroll_set_lines(&sel->crates, lines);
    scroll_set_lines(&sel->records, lines);
}


void selector_up(struct selector_t *sel)
{
    scroll_up(&sel->records, 1);
}


void selector_down(struct selector_t *sel)
{
    scroll_down(&sel->records, 1);
}


void selector_page_up(struct selector_t *sel)
{
    scroll_up(&sel->records, sel->records.lines);
}


void selector_page_down(struct selector_t *sel)
{
    scroll_down(&sel->records, sel->records.lines);
}


void selector_top(struct selector_t *sel)
{
    scroll_first(&sel->records);
}


void selector_bottom(struct selector_t *sel)
{
    scroll_last(&sel->records);
}


struct record_t* selector_current(struct selector_t *sel)
{
    int i;

    i = scroll_current(&sel->records);
    if (i == -1) {
        return NULL;
    } else {
        return sel->view_listing->record[i];
    }
}


/* When the crate has changed, update the current listing to reflect
 * the crate and the search criteria */

static void crate_has_changed(struct selector_t *sel)
{
    (void)listing_match(initial(sel), sel->view_listing, sel->search);
    scroll_set_entries(&sel->records, sel->view_listing->entries);
}


void selector_prev(struct selector_t *sel)
{
    scroll_up(&sel->crates, 1);
    sel->toggled = false;
    crate_has_changed(sel);
}


void selector_next(struct selector_t *sel)
{
    scroll_down(&sel->crates, 1);
    sel->toggled = false;
    crate_has_changed(sel);
}


/* Toggle between the current crate and the 'all' crate */

void selector_toggle(struct selector_t *sel)
{
    if (!sel->toggled) {
        sel->toggle_back = scroll_current(&sel->crates);
        scroll_first(&sel->crates);
        sel->toggled = true;
    } else {
        scroll_to(&sel->crates, sel->toggle_back);
        sel->toggled = false;
    }
    crate_has_changed(sel);
}


/* Expand the search. Do not disrupt the running process on memory
 * allocation failure, leave the view listing incomplete */

void selector_search_expand(struct selector_t *sel)
{
    if (sel->search_len == 0)
        return;

    sel->search[--sel->search_len] = '\0';

    (void)listing_match(initial(sel), sel->view_listing, sel->search);
    scroll_set_entries(&sel->records, sel->view_listing->entries);
}


/* Refine the search. Do not distrupt the running process on memory
 * allocation failure, leave the view listing incomplete */

void selector_search_refine(struct selector_t *sel, char key)
{
    struct listing_t *tmp;

    if (sel->search_len >= sizeof(sel->search) - 1) /* would overflow */
        return;

    sel->search[sel->search_len] = key;
    sel->search[++sel->search_len] = '\0';

    (void)listing_match(sel->view_listing, sel->swap_listing, sel->search);

    tmp = sel->view_listing;
    sel->view_listing = sel->swap_listing;
    sel->swap_listing = tmp;

    scroll_set_entries(&sel->records, sel->view_listing->entries);
}
