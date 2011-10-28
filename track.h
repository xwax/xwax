/*
 * Copyright (C) 2011 Mark Hills <mark@pogo.org.uk>
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

#include "import.h"
#include "list.h"

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
    int rate;
    pthread_mutex_t mx;

    /* pointers to external data */
   
    const char *importer, /* path to import script */
        *artist, *title;
    
    size_t bytes; /* loaded in */
    int length, /* track length in samples */
        blocks; /* number of blocks allocated */
    struct track_block_t *block[TRACK_MAX_BLOCKS];

    /* State of audio import */

    struct list rig;
    bool importing, has_poll;
    struct import_t import;

    /* Current value of audio meters when loading */
    
    unsigned short ppm;
    unsigned int overview;
};

void track_init(struct track_t *tr, const char *importer);
void track_clear(struct track_t *tr);

/* Functions used by the import operation */

void track_empty(struct track_t *tr);
void* track_access_pcm(struct track_t *tr, size_t *len);
void track_commit(struct track_t *tr, size_t len);

/* Functions used by the rig and main thread */

size_t track_pollfd(struct track_t *tr, struct pollfd *pe);
void track_handle(struct track_t *tr);
int track_import(struct track_t *tr, const char *path);

/* Return true if the track importer is running, otherwise false */

static inline bool track_is_importing(struct track_t *tr)
{
    return tr->importing;
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

