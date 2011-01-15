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

#ifndef RIG_H
#define RIG_H

#include <stdbool.h>

#include "track.h"

struct rig_t {
    int event[2]; /* pipe to wake up service thread */
};

int rig_main(struct rig_t *rig, struct track_t track[], size_t ntrack);
int rig_awaken(struct rig_t *rig);
int rig_quit(struct rig_t *rig);

#endif
