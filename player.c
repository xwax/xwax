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

#define SYNC_TIME (PLAYER_RATE / 2) /* time taken to reach sync */
#define SYNC_PITCH 0.05 /* don't sync at low pitches */


/* If the difference between our current position and that given by
 * the timecode is greater than this value, recover by jumping
 * straight to the position given by the timecode. */

#define SKIP_THRESHOLD (PLAYER_RATE / 8) /* before dropping audio */


/* Smooth the pitch returned from the timecoder. Smooth it too much
 * and we end up having to use sync to bend in line with the
 * timecode. Smooth too little and there will be audible
 * distortion. */

#define SMOOTHING (128 / DEVICE_FRAME) /* result value is in frames */


/* The base volume level. A value of 1.0 leaves no headroom to play
 * louder when the record is going faster than 1.0. */

#define VOLUME (7.0/8)


#define SQ(x) ((x)*(x))


/* Build a block of PCM audio, resampled from the track. Always builds
 * 'frame' samples, and returns the number of samples to advance the
 * track position by. This is just a basic resampler which has
 * particular problems where pitch > 1.0. */

static double build_pcm(signed short *pcm, int frame, struct track_t *tr,
                        double position, float pitch, float start_vol,
                        float end_vol)
{
    signed short a, b, *pa, *pb;
    int s, c;
    double ss;
    float f, vol, v;

    for(s = 0; s < frame; s++) {
        ss = position + (double)pitch * s;
        
        if(ss < 0 || ss >= tr->length) {
            for(c = 0; c < PLAYER_CHANNELS; c++)
                pcm[s * PLAYER_CHANNELS + c] = 0;
            
        } else {
            vol = end_vol + ((start_vol - end_vol) * s / frame);
            
            pa = track_get_sample(tr, (int)floor(ss));
            pb = track_get_sample(tr, (int)ceil(ss));
            
            f = ss - (floor)(ss);

            for(c = 0; c < PLAYER_CHANNELS; c++) {
                a = *(pa + c);
                b = *(pb + c);
                
                v = vol * ((1.0 - f) * a + f * b);
                
                pcm[s * PLAYER_CHANNELS + c] = v;
            }
        }
    }
    
    return (double)pitch * frame;
}


int player_init(struct player_t *pl)
{
    pl->lost = 0;
    pl->playing = 0;
    pl->reconnect = 0;

    pl->position = 0.0;
    pl->offset = 0;
    pl->target_position = -1;
    pl->last_difference = 0;

    pl->pitch = 1.0;
    pl->sync_pitch = 1.0;
    pl->volume = 1.0;
    pl->target_volume = pl->volume;

    pl->track = NULL;
    pl->timecoder = NULL;
    
    return 0;
}


int player_clear(struct player_t *pl)
{
    return 0;
}


int player_connect_timecoder(struct player_t *pl, struct timecoder_t *tc)
{
    pl->timecoder = tc;
    pl->reconnect = 1;
    pl->safe = timecoder_get_safe(tc);
    return 0;
}


int player_disconnect_timecoder(struct player_t *pl)
{
    pl->timecoder = NULL;
    return 0;
}


/* Synchronise this player to the timecode. This function calls
 * timecoder_get_pitch() and should be the only place which does for
 * any given timecoder_t */

int player_sync(struct player_t *pl)
{
    float pitch;
    unsigned int tcpos;
    signed int timecode;
    int when, alive, pitch_unavailable;

    if(!pl->timecoder)
        return 0;

    timecode = timecoder_get_position(pl->timecoder, &when);
    alive = timecoder_get_alive(pl->timecoder);
    pitch_unavailable = timecoder_get_pitch(pl->timecoder, &pitch);

    if(timecode != -1 && alive)
        pl->lost = 0;
    
    if(alive && !pl->lost)
        pl->playing = 1;
    
    if(!alive)
        pl->playing = 0;
    
    /* Automatically disconnect the timecoder if the needle is outside
     * the 'safe' zone of the record */

    if(timecode != -1 && timecode > pl->safe) {
        player_disconnect_timecoder(pl);
        return 0;
    }

    /* If the timecoder is alive and can tell us a current pitch based
     * on the sine wave, then use it */
    
    if(alive && !pitch_unavailable)
        pl->pitch = (pl->pitch * (SMOOTHING - 1) + pitch) / SMOOTHING;

    pl->target_volume = fabs(pl->pitch) * VOLUME;
    
    if(pl->target_volume > 1.0)
        pl->target_volume = 1.0;

    /* If we can read an absolute time from the timecode, then use it */
    
    if(timecode == -1)
        pl->target_position = -1;
    
    else {
        tcpos = (long long)timecode * TIMECODER_RATE
            / timecoder_get_resolution(pl->timecoder);

        pl->target_position = tcpos + pl->pitch * when;
        
        /* If reconnection has been requested, move the logical record
         * on the vinyl so that the current position is right under
         * the needle, and continue */

        if(pl->reconnect) {
            pl->offset = tcpos + pl->offset - pl->position;
            pl->reconnect = 0;
        }
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

int player_collect(struct player_t *pl, signed short *pcm, int samples)
{
    double diff;
    
    pl->sync_pitch = 1.0;

    if(pl->target_position != -1) {
        
        diff = pl->position - pl->target_position;
        pl->last_difference = diff; /* to print in user interface */
        
        if(fabs(diff) > SKIP_THRESHOLD) {

            /* Jump the track to the time */
            
            pl->position = pl->target_position;
            pl->sync_pitch = 1.0;
            
            fprintf(stderr, "Seek to new position %.0lf.\n", pl->position);
            
        } else if(fabs(pl->pitch) > SYNC_PITCH) {
            
            /* Pull the track into line. It wouldn't suprise me if
             * there is a mistake in here. A simplification of
             *
             * move = samples * pl->pitch;
             * end = diff - diff * samples / SYNC_TIME;
             * pl->sync_pitch = (move + end - diff) / move; */
            
            /* Divide by near-zero caught by pl->pitch > SYNC_PITCH */
            
            pl->sync_pitch = 1 - diff / (SYNC_TIME * pl->pitch);
        }
        
        /* Acknowledge that we've accounted for the target position */
        
        pl->target_position = -1;
    }
        
    pl->position += build_pcm(pcm, samples, pl->track,
                              pl->position - pl->offset,
                              pl->pitch * pl->sync_pitch,
                              pl->volume, pl->target_volume);
    
    pl->volume = pl->target_volume;

    return 0;
}


int player_connect_track(struct player_t *pl, struct track_t *tr)
{
    pl->track = tr;
    return 0;
}
