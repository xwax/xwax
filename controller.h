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

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdbool.h>
#include <stdlib.h>

struct deck_t;

/*
 * Base state of a 'controller', which is a MIDI controller or HID
 * device used to control the program
 */

struct controller_t {
    void *local;
    struct controller_type_t *type;
};

/*
 * Functions which must be implemented for a controller
 */

struct controller_type_t {
    int (*add_deck)(struct controller_t *c, struct deck_t *deck);
    int (*realtime)(struct controller_t *c);
    void (*clear)(struct controller_t *c);
};

void controller_init(struct controller_t *c, struct controller_type_t *t);
void controller_clear(struct controller_t *c);

void controller_add_deck(struct controller_t *c, struct deck_t *d);
void controller_handle(struct controller_t *c);

#endif
