/*
 * Copyright (C) 2011 Mark Hills <mark@pogo.org.uk>
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

void controller_init(struct controller_t *c, struct controller_type_t *t)
{
    debug("controller: init\n");
    c->type = t;
}

void controller_clear(struct controller_t *c)
{
    debug("controller: clear\n");
    c->type->clear(c);
}

/*
 * Add a deck to this controller, if possible
 */

void controller_add_deck(struct controller_t *c, struct deck_t *d)
{
    debug("controller: add_deck\n");

    if (c->type->add_deck(c, d) == 0) {
        debug("controller: one added\n");

        assert(d->ncontrol < sizeof d->control);
        d->control[d->ncontrol++] = c; /* for callbacks */
    }
}

void controller_handle(struct controller_t *c)
{
    if (c->type->realtime(c) != 0)
        abort();
}
