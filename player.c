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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "device.h"
#include "player.h"
#include "track.h"
#include "timecoder.h"


/* Bend playback speed to compensate for the difference between our
 * current position and that given by the timecode */

#define SYNC_TIME (1.0 / 2) /* time taken to reach sync */
#define SYNC_PITCH 0.05 /* don't sync at low pitches */
#define SYNC_RC 0.05 /* filter to 1.0 when no timecodes available */


/* If the difference between our current position and that given by
 * the timecode is greater than this value, recover by jumping
 * straight to the position given by the timecode. */

#define SKIP_THRESHOLD (1.0 / 8) /* before dropping audio */


/* The base volume level. A value of 1.0 leaves no headroom to play
 * louder when the record is going faster than 1.0. */

#define VOLUME (7.0/8)


#define SQ(x) ((x)*(x))


static inline float cubic_interpolate(float y[4], float mu)
{
    float a0, a1, a2, a3, mu2;

    mu2 = SQ(mu);
    a0 = y[3] - y[2] - y[0] + y[1];
    a1 = y[0] - y[1] - a0;
    a2 = y[2] - y[0];
    a3 = y[1];

    return (a0 * mu * mu2) + (a1 * mu2) + (a2 * mu) + a3;
}


/* Build a block of PCM audio, resampled from the track. Always builds
 * 'frame' samples, and returns the number of seconds to advance the
 * track position by. This is just a basic resampler which has
 * particular problems where pitch > 1.0. */

static double build_pcm(signed short *pcm, int samples, int rate,
                        struct track_t *tr, double position, float pitch,
                        float start_vol, float end_vol)
{
    int s;
    double sample, step;
    float vol, gradient;

    sample = position * tr->rate;
    step = (double)pitch * tr->rate / rate;

    vol = start_vol;
    gradient = (end_vol - start_vol) / samples;

    for (s = 0; s < samples; s++) {
        int c, sa, q;
        float f, i[PLAYER_CHANNELS][4];

        /* 4-sample window for interpolation */

        sa = (int)sample;
        if (sample < 0.0)
            sa--;
        f = sample - sa;
        sa--;

        for (q = 0; q < 4; q++, sa++) {
            if (sa < 0 || sa >= tr->length) {
                for (c = 0; c < PLAYER_CHANNELS; c++)
                    i[c][q] = 0.0;
            } else {
                signed short *ts;
                int c;

                ts = track_get_sample(tr, sa);
                for (c = 0; c < PLAYER_CHANNELS; c++)
                    i[c][q] = (float)ts[c];
            }
        }

        for (c = 0; c < PLAYER_CHANNELS; c++) {
            *pcm++ = vol * cubic_interpolate(i[c], f)
                + (float)(rand() % 32768) / 32768 - 0.5; /* dither */
        }

        sample += step;
        vol += gradient;
    }

    return (double)pitch * samples / rate;
}


void player_init(struct player_t *pl)
{
    pl->reconnect = 0;

    pl->position = 0.0;
    pl->offset = 0.0;
    pl->target_valid = 0;
    pl->last_difference = 0.0;

    pl->pitch = 0.0;
    pl->sync_pitch = 1.0;
    pl->volume = 0.0;

    pl->track = NULL;
    pl->timecoder = NULL;
}


void player_clear(struct player_t *pl)
{
}


void player_connect_timecoder(struct player_t *pl, struct timecoder_t *tc)
{
    pl->timecoder = tc;
    pl->reconnect = 1;
}


void player_disconnect_timecoder(struct player_t *pl)
{
    pl->timecoder = NULL;
}


static int sync_to_timecode(struct player_t *pl)
{
    float when;
    double tcpos;
    signed int timecode;

    timecode = timecoder_get_position(pl->timecoder, &when);

    /* Instruct the caller to disconnect the timecoder if the needle
     * is outside the 'safe' zone of the record */

    if (timecode != -1 && timecode > timecoder_get_safe(pl->timecoder))
        return -1;

    /* If the timecoder is alive, use the pitch from the sine wave */

    pl->pitch = timecoder_get_pitch(pl->timecoder);

    /* If we can read an absolute time from the timecode, then use it */
    
    if (timecode == -1)
	pl->target_valid = 0;

    else {
        tcpos = (double)timecode / timecoder_get_resolution(pl->timecoder);
        pl->target_position = tcpos + pl->pitch * when;
	pl->target_valid = 1;
    }

    return 0;
}


/* Return to the zero of the track */

int player_recue(struct player_t *pl)
{
    pl->offset = pl->position;
    return 0;
}


/* Get a block of PCM audio data to send to the soundcard. */

void player_collect(struct player_t *pl, signed short *pcm,
                    int samples, int rate)
{
    double diff;
    float dt, target_volume;

    dt = (float)samples / rate;

    if (pl->timecoder) {
        if (sync_to_timecode(pl) == -1)
            player_disconnect_timecoder(pl);
    }

    if (!pl->target_valid) {

        /* Without timecode sync, tend sync_pitch towards 1.0, to
         * avoid using outlier values from scratching for too long */

        pl->sync_pitch += dt / (SYNC_RC + dt) * (1.0 - pl->sync_pitch);

    } else {

        /* If reconnection has been requested, move the logical record
         * on the vinyl so that the current position is right under
         * the needle, and continue */

        if (pl->reconnect) {
            pl->offset += pl->target_position - pl->position;
	    pl->position = pl->target_position;
            pl->reconnect = 0;
        }

        /* Calculate the pitch compensation required to get us back on
         * track with the absolute timecode position */

        diff = pl->position - pl->target_position;
        pl->last_difference = diff; /* to print in user interface */
        
        if (fabs(diff) > SKIP_THRESHOLD) {

            /* Jump the track to the time */
            
            pl->position = pl->target_position;
            fprintf(stderr, "Seek to new position %.2lfs.\n", pl->position);

        } else if (fabs(pl->pitch) > SYNC_PITCH) {

            /* Re-calculate the drift between the timecoder pitch from
             * the sine wave and the timecode values */

            pl->sync_pitch = pl->pitch / (diff / SYNC_TIME + pl->pitch);

        }

        /* Acknowledge that we've accounted for the target position */
        
        pl->target_valid = 0;
    }

    target_volume = fabs(pl->pitch) * VOLUME;
    if (target_volume > 1.0)
        target_volume = 1.0;

    /* Sync pitch is applied post-filtering */

    pl->position += build_pcm(pcm, samples, rate,
			      pl->track,
                              pl->position - pl->offset,
                              pl->pitch * pl->sync_pitch,
                              pl->volume, target_volume);
    
    pl->volume = target_volume;
}


void player_connect_track(struct player_t *pl, struct track_t *tr)
{
    pl->track = tr;
}
