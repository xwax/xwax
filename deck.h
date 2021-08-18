/*
 * Copyright (C) 2021 Mark Hills <mark@xwax.org>
 *
 * This file is part of "xwax".
 *
 * "xwax" is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * "xwax" is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef DECK_H
#define DECK_H

#include <math.h>

#include "cues.h"
#include "device.h"
#include "index.h"
#include "player.h"
#include "realtime.h"
#include "timecoder.h"

#define NO_PUNCH (HUGE_VAL)

struct deck {
    struct device device;
    struct timecoder timecoder;
    const char *importer;
    bool protect;

    struct player player;
    const struct record *record;
    struct cues cues;

    /* Punch */

    double punch;

    /* A controller adds itself here */

    size_t ncontrol;
    struct controller *control[4];
};

int deck_init(struct deck *deck, struct rt *rt,
              struct timecode_def *timecode, const char *importer,
              double speed, bool phono, bool protect);
void deck_clear(struct deck *deck);

bool deck_is_locked(const struct deck *deck);

void deck_load(struct deck *deck, struct record *record);

void deck_recue(struct deck *deck);
void deck_clone(struct deck *deck, const struct deck *from);
void deck_unset_cue(struct deck *deck, unsigned int label);
void deck_cue(struct deck *deck, unsigned int label);
void deck_punch_in(struct deck *d, unsigned int label);
void deck_punch_out(struct deck *d);

#endif
