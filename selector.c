/*
 * Copyright (C) 2012 Mark Hills <mark@pogo.org.uk>,
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


static void scroll_reset(struct scroll *s)
{
    s->lines = 0;
    s->offset = 0;
    s->entries = 0;
    s->selected = -1;
}


/* Set the number of lines displayed on screen. The current selection
 * is moved to within range. */

static void scroll_set_lines(struct scroll *s, unsigned int lines)
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

static void scroll_set_entries(struct scroll *s, unsigned int entries)
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

static void scroll_up(struct scroll *s, unsigned int n)
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


static void scroll_down(struct scroll *s, unsigned int n)
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

static void scroll_first(struct scroll *s)
{
    s->selected = 0;
    s->offset = 0;
}


/* Scroll to the final entry on the list */

static void scroll_last(struct scroll *s)
{
    s->selected = s->entries - 1;
    s->offset = s->selected - s->lines + 1;
    if (s->offset < 0)
        s->offset = 0;
}


/* Scroll to an entry by index */

static void scroll_to(struct scroll *s, unsigned int n)
{
    int p;

    assert(s->selected != -1);
    assert(n < s->entries);

    /* Retain the on-screen position of the current selection */

    p = s->selected - s->offset;
    s->selected = n;
    s->offset = s->selected - p;

    if (s->offset < 0)
        s->offset = 0;
}


/* Return the index of the current selected list entry, or -1 if
 * no current selection */

static int scroll_current(struct scroll *s)
{
    if (s->entries == 0) {
        return -1;
    } else {
        return s->selected;
    }
}


/* Scroll to our target entry if it can be found, otherwise leave our
 * position unchanged */

static void retain_position(struct selector *sel)
{
    size_t n;
    struct listing *l;

    if (sel->target == NULL)
        return;

    l = sel->view_listing;

    switch (sel->sort) {
    case SORT_ARTIST:
    case SORT_BPM:
        n = listing_find(l, sel->target, sel->sort);
        break;
    case SORT_PLAYLIST:
        /* Linear search */
        for (n = 0; n < l->entries; n++) {
            if (l->record[n] == sel->target)
                break;
        }
        break;
    default:
        abort();
    }

    if (n < l->entries)
        scroll_to(&sel->records, n);
}


/* Return the listing which acts as the starting point before
 * string matching, based on the current crate */

static struct listing* initial(struct selector *sel)
{
    struct crate *c;

    c = sel->library->crate[sel->crates.selected];
    switch (sel->sort) {
    case SORT_ARTIST:
        return &c->by_artist;
    case SORT_BPM:
        return &c->by_bpm;
    case SORT_PLAYLIST:
        return &c->by_order;
    default:
        abort();
    }
}


void selector_init(struct selector *sel, struct library *lib)
{
    sel->library = lib;

    scroll_reset(&sel->records);
    scroll_reset(&sel->crates);

    scroll_set_entries(&sel->crates, lib->crates);

    sel->toggled = false;
    sel->sort = SORT_ARTIST;
    sel->search[0] = '\0';
    sel->search_len = 0;

    listing_init(&sel->listing_a);
    listing_init(&sel->listing_b);
    sel->view_listing = &sel->listing_a;
    sel->swap_listing = &sel->listing_b;

    (void)listing_copy(initial(sel), sel->view_listing);
    scroll_set_entries(&sel->records, sel->view_listing->entries);
}


void selector_clear(struct selector *sel)
{
    listing_clear(&sel->listing_a);
    listing_clear(&sel->listing_b);
}


void selector_set_lines(struct selector *sel, unsigned int lines)
{
    scroll_set_lines(&sel->crates, lines);
    scroll_set_lines(&sel->records, lines);
}

/*
 * Return: the currently selected record, or NULL if none selected
 */

struct record* selector_current(struct selector *sel)
{
    int i;

    i = scroll_current(&sel->records);
    if (i == -1) {
        return NULL;
    } else {
        return sel->view_listing->record[i];
    }
}


/* Make a note of the current selected record, and make it the
 * position we will try and retain if the crate is changed etc. */

static void set_target(struct selector *sel)
{
    struct record *x;

    x = selector_current(sel);
    if (x != NULL)
        sel->target = x;
}


void selector_up(struct selector *sel)
{
    scroll_up(&sel->records, 1);
    set_target(sel);
}


void selector_down(struct selector *sel)
{
    scroll_down(&sel->records, 1);
    set_target(sel);
}


void selector_page_up(struct selector *sel)
{
    scroll_up(&sel->records, sel->records.lines);
    set_target(sel);
}


void selector_page_down(struct selector *sel)
{
    scroll_down(&sel->records, sel->records.lines);
    set_target(sel);
}


void selector_top(struct selector *sel)
{
    scroll_first(&sel->records);
    set_target(sel);
}


void selector_bottom(struct selector *sel)
{
    scroll_last(&sel->records);
    set_target(sel);
}


/* When the crate has changed, update the current listing to reflect
 * the crate and the search criteria */

static void crate_has_changed(struct selector *sel)
{
    (void)listing_match(initial(sel), sel->view_listing, sel->search);
    scroll_set_entries(&sel->records, sel->view_listing->entries);
    retain_position(sel);
}


void selector_prev(struct selector *sel)
{
    scroll_up(&sel->crates, 1);
    sel->toggled = false;
    crate_has_changed(sel);
}


void selector_next(struct selector *sel)
{
    scroll_down(&sel->crates, 1);
    sel->toggled = false;
    crate_has_changed(sel);
}


/* Toggle between the current crate and the 'all' crate */

void selector_toggle(struct selector *sel)
{
    struct record *c;

    c = selector_current(sel);

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


/* Toggle between sort order */

void selector_toggle_order(struct selector *sel)
{
    set_target(sel);
    sel->sort = (sel->sort + 1) % SORT_END;
    crate_has_changed(sel);
}


/* Expand the search. Do not disrupt the running process on memory
 * allocation failure, leave the view listing incomplete */

void selector_search_expand(struct selector *sel)
{
    if (sel->search_len == 0)
        return;

    sel->search[--sel->search_len] = '\0';
    crate_has_changed(sel);
}


/* Refine the search. Do not distrupt the running process on memory
 * allocation failure, leave the view listing incomplete */

void selector_search_refine(struct selector *sel, char key)
{
    struct listing *tmp;

    if (sel->search_len >= sizeof(sel->search) - 1) /* would overflow */
        return;

    sel->search[sel->search_len] = key;
    sel->search[++sel->search_len] = '\0';

    (void)listing_match(sel->view_listing, sel->swap_listing, sel->search);

    tmp = sel->view_listing;
    sel->view_listing = sel->swap_listing;
    sel->swap_listing = tmp;

    scroll_set_entries(&sel->records, sel->view_listing->entries);
    set_target(sel);
}
