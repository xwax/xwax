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

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdbool.h>
#include <stdlib.h>

struct deck;

/*
 * Base state of a 'controller', which is a MIDI controller or HID
 * device used to control the program
 */

struct controller {
    bool fault;
    void *local;
    struct controller_ops *ops;
};

/*
 * Functions which must be implemented for a controller
 */

struct controller_ops {
    int (*add_deck)(struct controller *c, struct deck *deck);
    int (*realtime)(struct controller *c);
    void (*clear)(struct controller *c);
};

void controller_init(struct controller *c, struct controller_ops *t);
void controller_clear(struct controller *c);

void controller_add_deck(struct controller *c, struct deck *d);
void controller_handle(struct controller *c);

#endif
