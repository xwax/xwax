/*
 * Copyright (C) 2013 Mark Hills <mark@xwax.org>,
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

/* Scroll to our target entry if it can be found, otherwise leave our
 * position unchanged */

static void retain_position(struct selector *sel)
{
    size_t n;
    struct index *l;

    if (sel->target == NULL)
        return;

    l = sel->view_index;

    switch (sel->sort) {
    case SORT_ARTIST:
    case SORT_BPM:
        n = index_find(l, sel->target, sel->sort);
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
        listbox_to(&sel->records, n);
}

/* Return the index which acts as the starting point before
 * string matching, based on the current crate */

static struct index* initial(struct selector *sel)
{
    struct crate *c;

    c = sel->library->crate[sel->crates.selected];
    switch (sel->sort) {
    case SORT_ARTIST:
        return &c->listing.by_artist;
    case SORT_BPM:
        return &c->listing.by_bpm;
    case SORT_PLAYLIST:
        return &c->listing.by_order;
    default:
        abort();
    }
}

void selector_init(struct selector *sel, struct library *lib)
{
    sel->library = lib;

    listbox_init(&sel->records);
    listbox_init(&sel->crates);

    listbox_set_entries(&sel->crates, lib->crates);

    sel->toggled = false;
    sel->sort = SORT_ARTIST;
    sel->search[0] = '\0';
    sel->search_len = 0;

    index_init(&sel->index_a);
    index_init(&sel->index_b);
    sel->view_index = &sel->index_a;
    sel->swap_index = &sel->index_b;

    (void)index_copy(initial(sel), sel->view_index);
    listbox_set_entries(&sel->records, sel->view_index->entries);
}

void selector_clear(struct selector *sel)
{
    index_clear(&sel->index_a);
    index_clear(&sel->index_b);
}

void selector_set_lines(struct selector *sel, unsigned int lines)
{
    listbox_set_lines(&sel->crates, lines);
    listbox_set_lines(&sel->records, lines);
}

/*
 * Return: the currently selected record, or NULL if none selected
 */

struct record* selector_current(struct selector *sel)
{
    int i;

    i = listbox_current(&sel->records);
    if (i == -1) {
        return NULL;
    } else {
        return sel->view_index->record[i];
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
    listbox_up(&sel->records, 1);
    set_target(sel);
}

void selector_down(struct selector *sel)
{
    listbox_down(&sel->records, 1);
    set_target(sel);
}

void selector_page_up(struct selector *sel)
{
    listbox_up(&sel->records, sel->records.lines);
    set_target(sel);
}

void selector_page_down(struct selector *sel)
{
    listbox_down(&sel->records, sel->records.lines);
    set_target(sel);
}

void selector_top(struct selector *sel)
{
    listbox_first(&sel->records);
    set_target(sel);
}

void selector_bottom(struct selector *sel)
{
    listbox_last(&sel->records);
    set_target(sel);
}

/* When the crate has changed, update the current index to reflect
 * the crate and the search criteria */

static void crate_has_changed(struct selector *sel)
{
    (void)index_match(initial(sel), sel->view_index, sel->search);
    listbox_set_entries(&sel->records, sel->view_index->entries);
    retain_position(sel);
}

void selector_prev(struct selector *sel)
{
    listbox_up(&sel->crates, 1);
    sel->toggled = false;
    crate_has_changed(sel);
}

void selector_next(struct selector *sel)
{
    listbox_down(&sel->crates, 1);
    sel->toggled = false;
    crate_has_changed(sel);
}

/* Toggle between the current crate and the 'all' crate */

void selector_toggle(struct selector *sel)
{
    if (!sel->toggled) {
        sel->toggle_back = listbox_current(&sel->crates);
        listbox_first(&sel->crates);
        sel->toggled = true;
    } else {
        listbox_to(&sel->crates, sel->toggle_back);
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
 * allocation failure, leave the view index incomplete */

void selector_search_expand(struct selector *sel)
{
    if (sel->search_len == 0)
        return;

    sel->search[--sel->search_len] = '\0';
    crate_has_changed(sel);
}

/* Refine the search. Do not distrupt the running process on memory
 * allocation failure, leave the view index incomplete */

void selector_search_refine(struct selector *sel, char key)
{
    struct index *tmp;

    if (sel->search_len >= sizeof(sel->search) - 1) /* would overflow */
        return;

    sel->search[sel->search_len] = key;
    sel->search[++sel->search_len] = '\0';

    (void)index_match(sel->view_index, sel->swap_index, sel->search);

    tmp = sel->view_index;
    sel->view_index = sel->swap_index;
    sel->swap_index = tmp;

    listbox_set_entries(&sel->records, sel->view_index->entries);
    set_target(sel);
}
