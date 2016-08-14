/*
 * Copyright (C) 2016 Mark Hills <mark@xwax.org>
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

#include "oss.h"

#define FRAME 32 /* maximum read size */


struct oss {
    int fd;
    struct pollfd *pe;
    unsigned int rate;
};


static void clear(struct device *dv)
{
    int r;
    struct oss *oss = (struct oss*)dv->local;

    r = close(oss->fd);
    if (r == -1) {
        perror("close");
        abort();
    }

    free(dv->local);
}


/* Push audio into the device's buffer, for playback */

static int push(int fd, signed short *pcm, int samples)
{
    int r, bytes;

    bytes = samples * DEVICE_CHANNELS * sizeof(short);
    
    r = write(fd, pcm, bytes);

    if (r == -1) {
        perror("write");
        return -1;
    }
    
    if (r < bytes)
        fprintf(stderr, "Device output overrun.\n");

    return r / DEVICE_CHANNELS / sizeof(short);
}


/* Pull audio from the device, for recording */

static int pull(int fd, signed short *pcm, int samples)
{
    int r, bytes;

    bytes = samples * DEVICE_CHANNELS * sizeof(short);

    r = read(fd, pcm, bytes);

    if (r == -1) {
        perror("read");
        return -1;
    }

    if (r < bytes)
        fprintf(stderr, "Device input underrun.\n");

    return r / DEVICE_CHANNELS / sizeof(short);
}


static int handle(struct device *dv)
{
    signed short pcm[FRAME * DEVICE_CHANNELS];
    int samples;
    struct oss *oss = (struct oss*)dv->local;

    /* Check input buffer for recording */

    if (oss->pe->revents & POLLIN) {
        samples = pull(oss->fd, pcm, FRAME);
        if (samples == -1)
            return -1;
        device_submit(dv, pcm, samples);
    }

    /* Check the output buffer for playback */
    
    if (oss->pe->revents & POLLOUT) {
        device_collect(dv, pcm, FRAME);
        samples = push(oss->fd, pcm, FRAME);
        if (samples == -1)
            return -1;
    }

    return 0;
}


static ssize_t pollfds(struct device *dv, struct pollfd *pe, size_t z)
{
    struct oss *oss = (struct oss*)dv->local;

    if (z < 1)
        return -1;
    
    pe->fd = oss->fd;
    pe->events = POLLIN | POLLOUT;
    oss->pe = pe;

    return 1;
}


static unsigned int sample_rate(struct device *dv)
{
    struct oss *oss = (struct oss*)dv->local;

    return oss->rate;
}


static struct device_ops oss_ops = {
    .pollfds = pollfds,
    .handle = handle,
    .sample_rate = sample_rate,
    .clear = clear
};


int oss_init(struct device *dv, const char *filename, unsigned int rate,
	     unsigned short buffers, unsigned short fragment)
{
    int p, fd;
    struct oss *oss;
   
    fd = open(filename, O_RDWR, 0);
    if (fd == -1) {
        perror("open");
        return -1;
    }
    
    /* Ideally would set independent settings for the record and
     * playback buffers. Recording needs to buffer where necessary, as
     * lost audio results in zero-crossings which corrupt the
     * timecode. Playback buffer needs to be short to avoid high
     * latency */
    
    p = (buffers << 16) | fragment;
    if (ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &p) == -1) {
        perror("SNDCTL_DSP_SETFRAGMENT");
        goto fail;
    }

    p = AFMT_S16_LE;
    if (ioctl(fd, SNDCTL_DSP_SETFMT, &p) == -1) {
        perror("SNDCTL_DSP_SETFMT");
        goto fail;
    }

    p = DEVICE_CHANNELS;
    if (ioctl(fd, SNDCTL_DSP_CHANNELS, &p) == -1) {
        perror("SNDCTL_DSP_CHANNELS");
        goto fail;
    }

    p = rate;
    if (ioctl(fd, SNDCTL_DSP_SPEED, &p) == -1) {
        perror("SNDCTL_DSP_SPEED");
        goto fail;
    }

    p = fcntl(fd, F_GETFL);
    if (p == -1) {
        perror("F_GETFL");
        goto fail;
    }

    p |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, p) == -1) {
        perror("fcntl");
        return -1;
    }

    oss = malloc(sizeof *oss);
    if (oss == NULL) {
        perror("malloc");
        goto fail;
    }

    oss->fd = fd;
    oss->pe = NULL;
    oss->rate = rate;

    device_init(dv, &oss_ops);
    dv->local = oss;

    return 0;

 fail:
    close(fd);
    return -1;   
}
