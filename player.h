/*
 * Copyright (C) 2024 Mark Hills <mark@xwax.org>
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

#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>

#include "spin.h"
#include "track.h"

#define PLAYER_CHANNELS 2

struct player {
    double sample_dt;

    spin lock;
    struct track *track;

    /* Current playback parameters */

    double position, /* seconds */
        target_position, /* seconds, or TARGET_UNKNOWN */
        offset, /* track start point in timecode */
        last_difference, /* last known position minus target_position */
        pitch, /* from timecoder */
        sync_pitch, /* pitch required to sync to timecode signal */
        volume;

    /* Timecode control */

    struct timecoder *timecoder;
	struct mpu6050control *mpu6050;
    bool timecode_control,
        recalibrate; /* re-sync offset at next opportunity */
};

void player_init(struct player *pl, unsigned int sample_rate,
                 struct track *track, struct timecoder *timecoder, struct mpu6050control *mpu6050);
void player_clear(struct player *pl);

void player_set_timecoder(struct player *pl, struct timecoder *tc);
void player_set_timecode_control(struct player *pl, bool on);
bool player_toggle_timecode_control(struct player *pl);
void player_set_internal_playback(struct player *pl);

void player_set_track(struct player *pl, struct track *track);
void player_clone(struct player *pl, const struct player *from);

double player_get_position(struct player *pl);
double player_get_elapsed(struct player *pl);
double player_get_remain(struct player *pl);
bool player_is_active(const struct player *pl);

void player_seek_to(struct player *pl, double seconds);
void player_recue(struct player *pl);

void player_collect(struct player *pl, signed short *pcm, unsigned samples);

#endif
