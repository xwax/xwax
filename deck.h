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

#ifndef DECK_H
#define DECK_H

#include "cues.h"
#include "device.h"
#include "listing.h"
#include "player.h"
#include "realtime.h"
#include "timecoder.h"

struct deck {
    struct device device;
    struct timecoder timecoder;
    const char *importer;

    struct player player;
    const struct record *record;
    struct cues cues;

    /* A controller adds itself here */

    size_t ncontrol;
    struct controller *control[4];
};

int deck_init(struct deck *deck, struct rt *rt);
void deck_clear(struct deck *deck);

void deck_load(struct deck *deck, struct record *record);

void deck_unset_cue(struct deck *deck, unsigned int label);
void deck_cue(struct deck *deck, unsigned int label);

#endif
