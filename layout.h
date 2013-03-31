/*
 * Copyright (C) 2013 Mark Hills <mark@xwax.org>
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

/*
 * Layout functions for use by low-level UI code
 */

#ifndef LAYOUT_H
#define LAYOUT_H

#define LAYOUT_VERTICAL  0x01
#define LAYOUT_SECONDARY 0x02

typedef signed short pix_t;

struct rect {
    pix_t x, y, w, h; /* pixels */
};

struct layout {
    unsigned char flags;
    pix_t distance, space;
};

/*
 * Helper function to make layout specs
 */

static struct layout absolute(unsigned char flags, pix_t distance, pix_t space)
{
    struct layout l;

    l.flags = flags;
    l.distance = distance;
    l.space = space;

    return l;
}

static struct layout from_left(pix_t distance, pix_t space)
{
    return absolute(0, distance, space);
}

static struct layout from_right(pix_t distance, pix_t space)
{
    return absolute(LAYOUT_SECONDARY, distance, space);
}

static struct layout from_top(pix_t distance, pix_t space)
{
    return absolute(LAYOUT_VERTICAL, distance, space);
}

static struct layout from_bottom(pix_t distance, pix_t space)
{
    return absolute(LAYOUT_VERTICAL | LAYOUT_SECONDARY, distance, space);
}

/*
 * Create a new rectangle from pixels
 */

struct rect rect(pix_t x, pix_t y, pix_t w, pix_t h)
{
    struct rect r;

    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;

    return r;
}

/*
 * Apply a layout request to split two rectangles
 */

static void split(const struct rect in, const struct layout spec,
                  struct rect *a, struct rect *b)
{
    unsigned char flags;
    signed short p, q, full;
    struct rect discard;

    if (!a)
        a = &discard;
    if (!b)
        b = &discard;

    flags = spec.flags;

    if (flags & LAYOUT_VERTICAL)
        full = in.h;
    else
        full = in.w;

    if (flags & LAYOUT_SECONDARY) {
        p = full - spec.distance - spec.space;
        q = full - spec.distance;
    } else {
        p = spec.distance;
        q = spec.distance + spec.space;
    }

    if (flags & LAYOUT_VERTICAL) {
        *a = rect(in.x,     in.y,     in.w,     p);
        *b = rect(in.x,     in.y + q, in.w,     in.h - q);
    } else {
        *a = rect(in.x,     in.y,     p,        in.h);
        *b = rect(in.x + q, in.y,     in.w - q, in.h);
    }
}

#endif
