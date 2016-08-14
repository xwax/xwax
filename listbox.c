/*
 * Copyright (C) 2016 Mark Hills <mark@xwax.org>
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

#include "listbox.h"

void listbox_init(struct listbox *s)
{
    s->lines = 0;
    s->entries = 0;
    s->offset = 0;
    s->selected = -1;
}

/*
 * Set the number of lines displayed on screen
 *
 * Zero is valid, to mean that the display is hidden. In all other
 * cases, the current selection is moved to within range.
 */

void listbox_set_lines(struct listbox *s, unsigned int lines)
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

/*
 * Set the number of entries in the list which backs the scrolling
 * display. Bring the current selection within the bounds given.
 */

void listbox_set_entries(struct listbox *s, unsigned int entries)
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

/*
 * Scroll the selection up by n lines. Move the window offset if
 * needed
 */

void listbox_up(struct listbox *s, unsigned int n)
{
    s->selected -= n;

    if (s->selected < 0)
        s->selected = 0;

    /* Move the viewing offset up, if necessary */

    if (s->selected < s->offset) {
        s->offset = s->selected + s->lines / 2 - s->lines + 1;
        if (s->offset < 0)
            s->offset = 0;
    }
}

void listbox_down(struct listbox *s, unsigned int n)
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

/*
 * Scroll to the first entry on the list
 */

void listbox_first(struct listbox *s)
{
    s->selected = 0;
    s->offset = 0;
}

/*
 * Scroll to the final entry on the list
 */

void listbox_last(struct listbox *s)
{
    s->selected = s->entries - 1;
    s->offset = s->selected - s->lines + 1;

    if (s->offset < 0)
        s->offset = 0;
}

/*
 * Scroll to an entry by index
 */

void listbox_to(struct listbox *s, unsigned int n)
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

/*
 * Return the index of the current selected list entry, or -1 if
 * no current selection
 */

int listbox_current(const struct listbox *s)
{
    if (s->entries == 0)
        return -1;
    else
        return s->selected;
}

/*
 * Translate the given row on-screen (starting with row zero)
 * into a position in the list
 *
 * Return: -1 if the row is out of range, otherwise entry number
 */

int listbox_map(const struct listbox *s, int row)
{
    int e;

    assert(row >= 0);

    if (row >= s->lines)
        return -1;

    e = s->offset + row;
    if (e >= s->entries)
        return -1;

    return e;
}
