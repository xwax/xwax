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

#ifndef TRACK_H
#define TRACK_H

#include <pthread.h>
#include <stdbool.h>
#include <sys/poll.h>
#include <sys/types.h>

#include "rig.h"

#define TRACK_CHANNELS 2
#define TRACK_RATE 44100

#define TRACK_MAX_BLOCKS 64
#define TRACK_BLOCK_SAMPLES (2048 * 1024)
#define TRACK_PPM_RES 64
#define TRACK_OVERVIEW_RES 2048

struct track_block_t {
    signed short pcm[TRACK_BLOCK_SAMPLES * TRACK_CHANNELS];
    unsigned char ppm[TRACK_BLOCK_SAMPLES / TRACK_PPM_RES],
        overview[TRACK_BLOCK_SAMPLES / TRACK_OVERVIEW_RES];
};

struct track_t {
    int fd, rate;
    pid_t pid; /* 0 if not importing */
    struct pollfd *pe;
    pthread_mutex_t mx;
    struct rig_t *rig;

    /* pointers to external data */
   
    const char *importer, /* path to import script */
        *artist, *title;
    
    size_t bytes; /* loaded in */
    int length, /* track length in samples */
        blocks; /* number of blocks allocated */
    struct track_block_t *block[TRACK_MAX_BLOCKS];

    /* Current value of audio meters when loading */
    
    unsigned short ppm;
    unsigned int overview;
};

void track_init(struct track_t *tr, const char *importer);
void track_clear(struct track_t *tr);
int track_import(struct track_t *tr, const char *path);
int track_pollfd(struct track_t *tr, struct pollfd *pe);
int track_handle(struct track_t *tr);
int track_abort(struct track_t *tr);
int track_wait(struct track_t *tr);

/* Return true if the track importer is running, otherwise false */

static inline bool track_is_importing(struct track_t *tr)
{
    return (tr->pid != 0);
}

/* Return the pseudo-PPM meter value for the given sample */

static inline unsigned char track_get_ppm(struct track_t *tr, int s)
{
    struct track_block_t *b;
    b = tr->block[s / TRACK_BLOCK_SAMPLES];
    return b->ppm[(s % TRACK_BLOCK_SAMPLES) / TRACK_PPM_RES];
}

/* Return the overview meter value for the given sample */

static inline unsigned char track_get_overview(struct track_t *tr, int s)
{
    struct track_block_t *b;
    b = tr->block[s / TRACK_BLOCK_SAMPLES];
    return b->overview[(s % TRACK_BLOCK_SAMPLES) / TRACK_OVERVIEW_RES];
}

/* Return a pointer to (not value of) the sample data for each channel */

static inline signed short* track_get_sample(struct track_t *tr, int s)
{
    struct track_block_t *b;
    b = tr->block[s / TRACK_BLOCK_SAMPLES];
    return &b->pcm[(s % TRACK_BLOCK_SAMPLES) * TRACK_CHANNELS];
}

#endif

