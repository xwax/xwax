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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/soundcard.h>

#include "device.h"
#include "timecoder.h"
#include "player.h"


int device_open(struct device_t *dv, const char *filename,
                unsigned short buffers, unsigned short fragment)
{
    int p;
    
    dv->fd = open(filename, O_RDWR, 0);
    if(dv->fd == -1) {
        perror("open");
        return -1;
    }

    /* Ideally would set independent settings for the record and
     * playback buffers. Recording needs to buffer where necessary, as
     * lost audio results in zero-crossings which corrupt the
     * timecode. Playback buffer neews to be short to avoid high
     * latency */
    
    p = (buffers << 16) | fragment;
    if(ioctl(dv->fd, SNDCTL_DSP_SETFRAGMENT, &p) == -1) {
        perror("SNDCTL_DSP_SETFRAGMENT");
        goto fail;
    }

    p = AFMT_S16_LE;
    if(ioctl(dv->fd, SNDCTL_DSP_SETFMT, &p) == -1) {
        perror("SNDCTL_DSP_SETFMT");
        goto fail;
    }
    
    p = DEVICE_CHANNELS;
    if(ioctl(dv->fd, SNDCTL_DSP_CHANNELS, &p) == -1) {
        perror("SNDCTL_DSP_CHANNELS");
        goto fail;
    }

    p = DEVICE_RATE;
    if(ioctl(dv->fd, SNDCTL_DSP_SPEED, &p) == -1) {
        perror("SNDCTL_DSP_SPEED");
        goto fail;
    }

    if(fcntl(dv->fd, F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        return -1;
    }

    dv->pe = NULL;

    return dv->fd;

 fail:
    close(dv->fd);
    return -1;   
}


int device_close(struct device_t *dv)
{
    close(dv->fd);
    dv->fd = -1;

    return 0;
}


/* Push audio into the device's buffer, for playback */

int device_push(struct device_t *dv, signed short *pcm, int samples)
{
    int r, bytes;

    bytes = samples * DEVICE_CHANNELS * sizeof(short);
    
    r = write(dv->fd, pcm, bytes);

    if(r == -1) {
        perror("write");
        return -1;
    }
    
    if(r < bytes)
        fprintf(stderr, "Device output overrun.\n");

    return r / DEVICE_CHANNELS / sizeof(short);
}


/* Pull audio from the device, for recording */

int device_pull(struct device_t *dv, signed short *pcm, int samples)
{
    int r, bytes;

    bytes = samples * DEVICE_CHANNELS * sizeof(short);

    r = read(dv->fd, pcm, bytes);

    if(r == -1) {
        perror("read");
        return -1;
    }

    if(r < bytes) 
        fprintf(stderr, "Device input underrun.\n");

    return r / DEVICE_CHANNELS / sizeof(short);
}


int device_handle(struct device_t *dv)
{
    signed short pcm[DEVICE_FRAME * DEVICE_CHANNELS];
    int samples;

    /* Check input buffer for recording */

    if(!dv->pe || dv->pe->revents & POLLIN) {
        samples = device_pull(dv, pcm, DEVICE_FRAME);
        if(samples == -1)
            return -1;
        
        if(dv->timecoder) {
            timecoder_submit(dv->timecoder, pcm, samples);

            if(dv->player)
                player_sync(dv->player);
        }
    }

    /* Check the output buffer for playback */
    
    if((!dv->pe || dv->pe->revents & POLLOUT) && dv->player) {

        /* Always push some audio to the soundcard, even if it means
         * silence. This has shown itself to be much more reliable
         * than constantly starting and stopping -- which can affect
         * other devices to the one which is doing the stopping. */
        
        if(dv->player->playing)
            player_collect(dv->player, pcm, DEVICE_FRAME);
        else
            memset(pcm, 0, DEVICE_FRAME * DEVICE_CHANNELS * sizeof(short));
        
        samples = device_push(dv, pcm, DEVICE_FRAME);
        
        if(samples == -1)
            return -1;
    }

    return 0;
}
