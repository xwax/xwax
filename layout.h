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

/*
 * Layout functions for use by low-level UI code
 */

#ifndef LAYOUT_H
#define LAYOUT_H

#define LAYOUT_VERTICAL  0x01
#define LAYOUT_SECONDARY 0x02
#define LAYOUT_PIXELS    0x04 /* default is our own screen units */

typedef signed short pix_t;

/*
 * A rectangle defines an on-screen area, and other attributes
 * for anything that gets into the area
 */

struct rect {
    pix_t x, y, w, h; /* pixels */
    float scale;
};

struct layout {
    unsigned char flags;
    float portion;
    unsigned int distance, space; /* may be pixels, or units */
};

/*
 * Helper function to make layout specs
 */

static inline struct layout absolute(unsigned char flags, unsigned int distance,
                                     unsigned int space)
{
    struct layout l;

    l.flags = flags;
    l.portion = 0.0;
    l.distance = distance;
    l.space = space;

    return l;
}

static inline struct layout from_left(unsigned int distance,
                                      unsigned int space)
{
    return absolute(0, distance, space);
}

static inline struct layout from_right(unsigned int distance,
                                       unsigned int space)
{
    return absolute(LAYOUT_SECONDARY, distance, space);
}

static inline struct layout from_top(unsigned int distance,
                                     unsigned int space)
{
    return absolute(LAYOUT_VERTICAL, distance, space);
}

static inline struct layout from_bottom(unsigned int distance,
                                        unsigned int space)
{
    return absolute(LAYOUT_VERTICAL | LAYOUT_SECONDARY, distance, space);
}

static inline struct layout portion(unsigned char flags, double f,
                                    unsigned int space)
{
    struct layout l;

    l.flags = flags;
    l.portion = f;
    l.distance = 0;
    l.space = space;

    return l;
}

static inline struct layout columns(unsigned int n, unsigned int total,
                                    unsigned int space)
{
    assert(n < total);
    return portion(0, 1.0 / (total - n), space);
}

static inline struct layout rows(unsigned int n, unsigned int total,
                                 unsigned int space)
{
    assert(n < total);
    return portion(LAYOUT_VERTICAL, 1.0 / (total - n), space);
}

/*
 * Take an existing layout spec and request that it be in pixels
 *
 * Most dimensions are done in terms of 'screen units' but sometimes
 * we need to apply layout based on a pixel measurement (eg.  returned
 * to us when drawing text)
 */

static inline struct layout pixels(struct layout j)
{
    struct layout r;

    r = j;
    r.flags |= LAYOUT_PIXELS;

    return r;
}

/*
 * Create a new rectangle from pixels
 */

static inline struct rect rect(pix_t x, pix_t y, pix_t w, pix_t h, float scale)
{
    struct rect r;

    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    r.scale = scale;

    return r;
}

/*
 * Apply a layout request to split two rectangles
 *
 * The caller is allowed to use the same rectangle for output
 * as is the input.
 */

static inline void split(const struct rect x, const struct layout spec,
                         struct rect *a, struct rect *b)
{
    unsigned char flags;
    signed short p, q, full, distance, space;
    struct rect discard, in;

    in = x; /* allow caller to re-use x as an output */

    if (!a)
        a = &discard;
    if (!b)
        b = &discard;

    flags = spec.flags;

    if (flags & LAYOUT_VERTICAL)
        full = in.h;
    else
        full = in.w;

    if (flags & LAYOUT_PIXELS) {
        space = spec.space;
        distance = spec.distance;
    } else {
        space = spec.space * in.scale;
        distance = spec.distance * in.scale;
    }

    if (spec.portion != 0.0)
        distance = spec.portion * full - space / 2;

    if (flags & LAYOUT_SECONDARY) {
        p = full - distance - space;
        q = full - distance;
    } else {
        p = distance;
        q = distance + space;
    }

    if (flags & LAYOUT_VERTICAL) {
        *a = rect(in.x,     in.y,     in.w,     p,        in.scale);
        *b = rect(in.x,     in.y + q, in.w,     in.h - q, in.scale);
    } else {
        *a = rect(in.x,     in.y,     p,        in.h,     in.scale);
        *b = rect(in.x + q, in.y,     in.w - q, in.h,     in.scale);
    }
}

/*
 * Shrink a rectangle to leave a border on all sides
 */

static inline struct rect shrink(const struct rect in, int distance)
{
    struct rect out;

    out = in;

    distance *= in.scale;

    if (distance * 2 < in.w) {
        out.x = in.x + distance;
        out.w = in.w - distance * 2;
    }

    if (distance * 2 < in.h) {
        out.y = in.y + distance;
        out.h = in.h - distance * 2;
    }

    return out;
}

/*
 * Calculate the number of lines we can expect to fit if we
 * do splits of the given row height
 */

static inline unsigned int count_rows(struct rect in, unsigned int row_height)
{
    unsigned int pixels;

    pixels = row_height * in.scale;
    return in.h / pixels;
}

#endif
