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


struct oss_t {
    int fd;
    struct pollfd *pe;
};


static int clear(struct device_t *dv)
{
    int r;
    struct oss_t *oss = (struct oss_t*)dv->local;

    r = close(oss->fd);
    if(r == -1)
        perror("close");

    free(dv->local);
    
    return r;
}


/* Push audio into the device's buffer, for playback */

static int push(int fd, signed short *pcm, int samples)
{
    int r, bytes;

    bytes = samples * DEVICE_CHANNELS * sizeof(short);
    
    r = write(fd, pcm, bytes);

    if(r == -1) {
        perror("write");
        return -1;
    }
    
    if(r < bytes)
        fprintf(stderr, "Device output overrun.\n");

    return r / DEVICE_CHANNELS / sizeof(short);
}


/* Pull audio from the device, for recording */

static int pull(int fd, signed short *pcm, int samples)
{
    int r, bytes;

    bytes = samples * DEVICE_CHANNELS * sizeof(short);

    r = read(fd, pcm, bytes);

    if(r == -1) {
        perror("read");
        return -1;
    }

    if(r < bytes) 
        fprintf(stderr, "Device input underrun.\n");

    return r / DEVICE_CHANNELS / sizeof(short);
}


static int handle(struct device_t *dv)
{
    signed short pcm[DEVICE_FRAME * DEVICE_CHANNELS];
    int samples;
    struct oss_t *oss = (struct oss_t*)dv->local;

    /* Check input buffer for recording */

    if(oss->pe->revents & POLLIN) {
        samples = pull(oss->fd, pcm, DEVICE_FRAME);
        if(samples == -1)
            return -1;
        
        if(dv->timecoder) {
            timecoder_submit(dv->timecoder, pcm, samples);

            if(dv->player)
                player_sync(dv->player);
        }
    }

    /* Check the output buffer for playback */
    
    if((oss->pe->revents & POLLOUT) && dv->player) {

        /* Always push some audio to the soundcard, even if it means
         * silence. This has shown itself to be much more reliable
         * than constantly starting and stopping -- which can affect
         * other devices to the one which is doing the stopping. */
        
        if(dv->player->playing)
            player_collect(dv->player, pcm, DEVICE_FRAME);
        else
            memset(pcm, 0, DEVICE_FRAME * DEVICE_CHANNELS * sizeof(short));
        
        samples = push(oss->fd, pcm, DEVICE_FRAME);
        
        if(samples == -1)
            return -1;
    }

    return 0;
}


int pollfds(struct device_t *dv, struct pollfd *pe, int n)
{
    struct oss_t *oss = (struct oss_t*)dv->local;

    if(n < 1)
        return -1;
    
    pe->fd = oss->fd;
    pe->events = POLLIN | POLLOUT | POLLHUP;

    oss->pe = pe;

    return 1;
}


int oss_init(struct device_t *dv, const char *filename,
             unsigned short buffers, unsigned short fragment)
{
    int p, fd;
    struct oss_t *oss;
   
    fd = open(filename, O_RDWR, 0);
    if(fd == -1) {
        perror("open");
        return -1;
    }
    
    /* Ideally would set independent settings for the record and
     * playback buffers. Recording needs to buffer where necessary, as
     * lost audio results in zero-crossings which corrupt the
     * timecode. Playback buffer neews to be short to avoid high
     * latency */
    
    p = (buffers << 16) | fragment;
    if(ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &p) == -1) {
        perror("SNDCTL_DSP_SETFRAGMENT");
        goto fail;
    }

    p = AFMT_S16_LE;
    if(ioctl(fd, SNDCTL_DSP_SETFMT, &p) == -1) {
        perror("SNDCTL_DSP_SETFMT");
        goto fail;
    }
    
    p = DEVICE_CHANNELS;
    if(ioctl(fd, SNDCTL_DSP_CHANNELS, &p) == -1) {
        perror("SNDCTL_DSP_CHANNELS");
        goto fail;
    }

    p = DEVICE_RATE;
    if(ioctl(fd, SNDCTL_DSP_SPEED, &p) == -1) {
        perror("SNDCTL_DSP_SPEED");
        goto fail;
    }

    if(fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        return -1;
    }

    dv->local = malloc(sizeof(struct oss_t));
    if(!dv->local) {
        perror("malloc");
        goto fail;
    }

    oss = (struct oss_t*)dv->local;

    oss->fd = fd;
    oss->pe = NULL;

    dv->pollfds = pollfds;
    dv->handle = handle;
    dv->start = NULL;
    dv->stop = NULL;
    dv->clear = clear;

    return 0;

 fail:
    close(fd);
    return -1;   
}