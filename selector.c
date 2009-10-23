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

#include <stdlib.h>

#include "selector.h"


void selector_init(struct selector_t *sel, struct library_t *lib)
{
    sel->library = lib;
    sel->base_listing = &library_get_crate(sel->library, CRATE_ALL)->listing;

    sel->lst_lines = 0;
    sel->lst_selected = 0;
    sel->lst_offset = 0;
    sel->cr_lines = 0;
    sel->cr_selected = 0;
    sel->cr_offset = 0;

    sel->search[0] = '\0';
    sel->search_len = 0;

    listing_init(&sel->listing_a);
    listing_init(&sel->listing_b);
    sel->view_listing = &sel->listing_a;
    sel->swap_listing = &sel->listing_b;

    listing_blank(sel->view_listing);
    listing_match(sel->base_listing, sel->view_listing, sel->search);
}


void selector_clear(struct selector_t *sel)
{
    listing_clear(&sel->listing_a);
    listing_clear(&sel->listing_b);
}


static void check_up_constraints(int lines, int *selected, int *offset)
{
    if(*selected < 0)
        *selected = 0;

    if(*selected < *offset)
        *offset -= (*offset - *selected) + (lines / 2);

    if(*offset < 0)
        *offset = 0;
}


static void check_down_constraints(int size, int lines,
                                   int *selected, int *offset)
{
    if(*selected > *offset + lines - 1)
        *offset = *selected - lines / 2;

    if(*selected >= size)
        *selected = size - 1;

    if(*offset > size - lines)
        *offset = size - lines;
}


void selector_lst_prev(struct selector_t *sel, int count)
{
    sel->lst_selected -= count;
    check_up_constraints(sel->lst_lines, &sel->lst_selected, &sel->lst_offset);
}


void selector_lst_next(struct selector_t *sel, int count)
{
    sel->lst_selected += count;
    check_down_constraints(sel->view_listing->entries, sel->lst_lines,
                           &sel->lst_selected, &sel->lst_offset);
}


struct record_t* selector_lst_current(struct selector_t *sel)
{
    struct record_t *record = NULL;

    if((sel->lst_selected != -1) && (sel->view_listing->entries > 0))
        record = sel->view_listing->record[sel->lst_selected];

    return record;
}


static void change_listing(struct selector_t *sel)
{
    sel->lst_lines = 0;
    sel->lst_offset = 0;
    sel->lst_selected = 0;

    listing_blank(sel->view_listing);
    sel->base_listing = &sel->library->crate[sel->cr_selected]->listing;
    listing_match(sel->base_listing, sel->view_listing, sel->search);
}


void selector_cr_prev(struct selector_t *sel, int count)
{
    sel->cr_selected -= count;
    check_up_constraints(sel->cr_lines, &sel->cr_selected, &sel->cr_offset);
    change_listing(sel);
}


void selector_cr_next(struct selector_t *sel, int count)
{
    sel->cr_selected += count;
    check_down_constraints(sel->library->crates, sel->cr_lines,
                           &sel->cr_selected, &sel->cr_offset);
    change_listing(sel);
}


struct crate_t* selector_cr_current(struct selector_t *sel)
{
    struct crate_t *crate = NULL;

    if(sel->cr_selected != -1)
        crate = sel->library->crate[sel->cr_selected];

    return crate;
}


void selector_search_expand(struct selector_t *sel)
{
    if(sel->search_len > 0)
        sel->search[--sel->search_len] = '\0';

    listing_blank(sel->view_listing);
    listing_match(sel->base_listing, sel->view_listing, sel->search);

    if((sel->lst_selected < 0) && (sel->view_listing->entries > 0))
        sel->lst_selected = 0;
}


void selector_search_refine(struct selector_t *sel, char key)
{
    struct listing_t *lst_tmp;

    sel->search[sel->search_len] = key;
    sel->search[++sel->search_len] = '\0';

    listing_blank(sel->swap_listing);
    listing_match(sel->view_listing, sel->swap_listing, sel->search);

    lst_tmp = sel->view_listing;
    sel->view_listing = sel->swap_listing;
    sel->swap_listing = lst_tmp;

    sel->lst_offset = 0;

    if(sel->lst_selected >= sel->lst_lines)
        sel->lst_selected = sel->lst_lines - 1;

    if(sel->lst_selected >= sel->view_listing->entries)
        sel->lst_selected = sel->view_listing->entries - 1;
}
