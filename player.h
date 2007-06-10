/*
 * Copyright (C) 2007 Mark Hills <mark@pogo.org.uk>
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

#ifndef PLAYER_H
#define PLAYER_H

#include "track.h"

#define PLAYER_RATE 44100
#define PLAYER_CHANNELS 2

struct player_t {
    int lost, /* Position no longer predictable from speed alone */
        playing, /* Not silence */
        reconnect; /* Re-sync the offset at next opportunity */

    /* Current playback parameters */
    
    double position,
        last_difference; /* last known position minus target_position */
    float pitch, /* pitch from turntable */
        sync_pitch, /* pitch required to sync to timecode signal */
        volume, target_volume;

    signed int target_position,
        offset, /* track start point in timecode */
        safe; /* copied from timecoder_t */

    struct track_t *track;
    struct timecoder_t *timecoder;
};

int player_init(struct player_t *pl);
int player_clear(struct player_t *pl);

int player_connect_timecoder(struct player_t *pl, struct timecoder_t *tc);
int player_disconnect_timecoder(struct player_t *pl);
int player_sync(struct player_t *pl);

int player_control(struct player_t *pl, float pitch, float volume,
                   int timecode_known, double target_position);
int player_recue(struct player_t *pl);

int player_collect(struct player_t *pl, signed short *pcm, int samples);

int player_connect_track(struct player_t *pl, struct track_t *tr);

#endif
