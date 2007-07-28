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

#ifndef DEVICE_H
#define DEVICE_H

#include <alsa/asoundlib.h>
#include <sys/poll.h>

#define DEVICE_CHANNELS 2
#define DEVICE_RATE 44100

/* This structure doesn't have corresponding functions to be an
 * abstraction of the ALSA calls; it is merely a container for these
 * variables. */

struct alsa_pcm_t {
    snd_pcm_t *pcm;

    struct pollfd *pe;
    int pe_count; /* number of pollfd entries */

    signed short *buf;
    snd_pcm_uframes_t period;
};

struct device_t {
    struct alsa_pcm_t capture, playback;

    struct timecoder_t *timecoder;
    struct player_t *player;
};

int device_open(struct device_t *dv, const char *filename,
                unsigned short buffers, unsigned short fragment);
int device_start(struct device_t *dv);
int device_close(struct device_t *dv);
int device_fill_pollfd(struct device_t *dv, struct pollfd *pe, int pe_size);
int device_handle(struct device_t *dv);

#endif
