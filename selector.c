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


void selector_init(struct selector_t *sel, struct listing_t *lst)
{
    sel->base_listing = lst;

    sel->lst_lines = 0;
    sel->lst_selected = 0;
    sel->lst_offset = 0;

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


void selector_lst_prev(struct selector_t *sel, int count)
{
    sel->lst_selected -= count;

    if(sel->lst_selected < 0)
        sel->lst_selected = 0;

    if(sel->lst_selected < sel->lst_offset) {
        sel->lst_offset -= (sel->lst_offset - sel->lst_selected)
                                + (sel->lst_lines / 2);
    }

    if(sel->lst_offset < 0)
        sel->lst_offset = 0;
}


void selector_lst_next(struct selector_t *sel, int count)
{
    sel->lst_selected += count;

    if(sel->lst_selected > sel->lst_offset + sel->lst_lines - 1)
        sel->lst_offset = sel->lst_selected - sel->lst_lines / 2;

    if(sel->lst_selected >= sel->view_listing->entries)
        sel->lst_selected = sel->view_listing->entries - 1;

    if(sel->lst_offset > sel->view_listing->entries - sel->lst_lines)
        sel->lst_offset = sel->view_listing->entries - sel->lst_lines;
}


struct record_t* selector_lst_current(struct selector_t *sel)
{
    struct record_t *record = NULL;

    if((sel->lst_selected != -1) && (sel->view_listing->entries > 0))
        record = sel->view_listing->record[sel->lst_selected];

    return record;
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
