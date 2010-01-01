/*
 * Copyright (C) 2010 Mark Hills <mark@pogo.org.uk>
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

#include "timecoder.h"
#include "library.h"

#define MAX_PLAYERS 4
#define MAX_TIMECODERS 4

struct interface_t {
    short int players, timecoders;
    
    struct player_t *player[MAX_PLAYERS];
    struct timecoder_t *timecoder[MAX_TIMECODERS];
    struct library_t *library;
};

void interface_init(struct interface_t *in);
int interface_run(struct interface_t *in);

#endif
