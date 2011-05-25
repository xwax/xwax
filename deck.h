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
#include "player.h"
#include "rig.h"
#include "realtime.h"
#include "timecoder.h"
#include "track.h"

struct deck_t {
    struct device_t device;
    struct timecoder_t timecoder;
    struct track_t track;

    struct cues_t cues;
    struct player_t player;

    /* A controller adds itself here */

    size_t ncontrol;
    struct controller_t *control[4];
};

int deck_init(struct deck_t *deck, struct rt_t *rt, struct rig_t *rig);
void deck_clear(struct deck_t *deck);

void deck_unset_cue(struct deck_t *deck, unsigned int label);
void deck_cue(struct deck_t *deck, unsigned int label);

#endif
