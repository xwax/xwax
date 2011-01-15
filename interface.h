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

#ifndef INTERFACE_H
#define INTERFACE_H

#include <pthread.h>

#include "timecoder.h"
#include "library.h"
#include "rig.h"
#include "selector.h"

#define MAX_DECKS 3

struct interface_t {
    pthread_t ph;

    size_t players;
    struct player_t *player[MAX_DECKS];

    size_t timecoders;
    struct timecoder_t *timecoder[MAX_DECKS];

    struct rig_t *rig;
    struct selector_t selector;
};

int interface_start(struct interface_t *in, size_t ndeck,
                    struct player_t *pl, struct timecoder_t *tc,
                    struct library_t *lib, struct rig_t *rig);
void interface_stop(struct interface_t *in);

#endif
