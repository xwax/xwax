/*
 * Copyright (C) 2012 Mark Hills <mark@pogo.org.uk>
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

#include "controller.h"
#include "deck.h"
#include "debug.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))

void controller_init(struct controller *c, struct controller_ops *ops)
{
    debug("%p", c);
    c->fault = false;
    c->ops = ops;
}

void controller_clear(struct controller *c)
{
    debug("%p", c);
    c->ops->clear(c);
}

/*
 * Add a deck to this controller, if possible
 */

void controller_add_deck(struct controller *c, struct deck *d)
{
    debug("%p adding deck %p", c, d);

    if (c->ops->add_deck(c, d) == 0) {
        debug("deck was added");

        assert(d->ncontrol < ARRAY_SIZE(d->control)); /* FIXME: report error */
        d->control[d->ncontrol++] = c; /* for callbacks */
    }
}

void controller_handle(struct controller *c)
{
    if (c->fault)
        return;

    if (c->ops->realtime(c) != 0) {
        c->fault = true;
        fputs("Error handling hardware controller; disabling it\n", stderr);
    }
}
