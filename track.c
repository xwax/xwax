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

#define _BSD_SOURCE /* vfork() */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "realtime.h"
#include "rig.h"
#include "track.h"

#define SAMPLE (sizeof(signed short) * TRACK_CHANNELS) /* bytes per sample */
#define TRACK_BLOCK_PCM_BYTES (TRACK_BLOCK_SAMPLES * SAMPLE)

/*
 * An empty track is used rarely, and is easier than
 * continuous checks for NULL throughout the code
 */

static struct track_t empty = {
    .refcount = 1,

    .rate = TRACK_RATE,
    .artist = "Empty",
    .title = "Empty",

    .bytes = 0,
    .length = 0,
    .blocks = 0,

    .importing = false
};

/*
 * Allocate more memory
 *
 * Return: -1 if memory could not be allocated, otherwize 0
 */

static int more_space(struct track_t *tr)
{
    struct track_block_t *block;

    rt_not_allowed();

    if (tr->blocks >= TRACK_MAX_BLOCKS) {
        fprintf(stderr, "Maximum track length reached.\n");
        return -1;
    }

    block = malloc(sizeof(struct track_block_t));
    if (block == NULL) {
        perror("malloc");
        return -1;
    }

    tr->block[tr->blocks++] = block;

    fprintf(stderr, "Allocated new track block (%d blocks, %zu bytes).\n",
            tr->blocks, tr->blocks * TRACK_BLOCK_SAMPLES * SAMPLE);

    return 0;
}

/*
 * Get access to the PCM buffer for incoming audio
 *
 * Return: pointer to buffer
 * Post: len contains the length of the buffer, in bytes
 */

void* track_access_pcm(struct track_t *tr, size_t *len)
{
    unsigned int block;
    size_t fill;

    block = tr->bytes / TRACK_BLOCK_PCM_BYTES;
    if (block == tr->blocks) {
        if (more_space(tr) == -1)
            return NULL;
    }

    fill = tr->bytes % TRACK_BLOCK_PCM_BYTES;
    *len = TRACK_BLOCK_PCM_BYTES - fill;

    return (void*)tr->block[block]->pcm + fill;
}

/*
 * Notify that audio has been placed in the buffer
 *
 * The parameter is the number of stereo samples which have been
 * placed in the buffer.
 *
 * Pre: the number of samples does not overflow the size of the buffer,
 * given by track_access_pcm()
 */

static void commit_pcm_samples(struct track_t *tr, unsigned int samples)
{
    unsigned int fill;
    signed short *pcm;
    struct track_block_t *block;

    block = tr->block[tr->length / TRACK_BLOCK_SAMPLES];
    fill = tr->length % TRACK_BLOCK_SAMPLES;
    pcm = block->pcm + TRACK_CHANNELS * fill;

    assert(samples <= TRACK_BLOCK_SAMPLES - fill);
    tr->length += samples;

    /* Meter the new audio */

    while (samples > 0) {
        unsigned short v;
        unsigned int w;

        v = abs(pcm[0]) + abs(pcm[1]);

        /* PPM-style fast meter approximation */

        if (v > tr->ppm)
            tr->ppm += (v - tr->ppm) >> 3;
        else
            tr->ppm -= (tr->ppm - v) >> 9;

        block->ppm[fill / TRACK_PPM_RES] = tr->ppm >> 8;

        /* Update the slow-metering overview. Fixed point arithmetic
         * going on here */

        w = v << 16;

        if (w > tr->overview)
            tr->overview += (w - tr->overview) >> 8;
        else
            tr->overview -= (tr->overview - w) >> 17;

        block->overview[fill / TRACK_OVERVIEW_RES] = tr->overview >> 24;

        fill++;
        pcm += TRACK_CHANNELS;
        samples--;
    }
}

/*
 * Notify that data has been placed in the buffer
 *
 * This function passes any whole samples to commit_pcm_samples()
 * and leaves the residual in the buffer ready for next time.
 *
 * Pre: len is not greater than the size of the buffer, available
 * from track_access_pcm()
 */

void track_commit(struct track_t *tr, size_t len)
{
    tr->bytes += len;
    commit_pcm_samples(tr, tr->bytes / SAMPLE - tr->length);
}

/*
 * Initialise object which will hold PCM audio data, and start
 * importing the data
 *
 * Post: track is initialised
 */

static int track_init(struct track_t *t, const char *importer,
                       const char *path)
{
    t->refcount = 0;

    t->artist = NULL;
    t->title = NULL;

    t->blocks = 0;
    t->rate = TRACK_RATE;

    t->bytes = 0;
    t->length = 0;
    t->ppm = 0;
    t->overview = 0;

    t->importing = true;
    t->has_poll = false;

    if (import_start(&t->import, t, importer, path) == -1)
        return -1;

    rig_post_track(t);

    return 0;
}

/*
 * Destroy this track from memory
 *
 * Terminates any import processes and frees any memory allocated by
 * this object.
 *
 * Pre: track is initialised
 */

static void track_clear(struct track_t *tr)
{
    int n;

    assert(!tr->importing);

    for (n = 0; n < tr->blocks; n++)
        free(tr->block[n]);
}

/*
 * Get a pointer to a track object for the given importer and path
 *
 * Return: pointer, or NULL if not enough resources
 */

struct track_t* track_get_by_import(const char *importer, const char *path)
{
    struct track_t *t;

    t = malloc(sizeof *t);
    if (t == NULL) {
        perror("malloc");
        return NULL;
    }

    if (track_init(t, importer, path) == -1) {
        free(t);
        return NULL;
    }

    t->refcount++;

    return t;
}

/*
 * Get a pointer to a static track containing no audio
 *
 * Return: pointer
 */

struct track_t* track_get_empty(void)
{
    empty.refcount++;
    return &empty;
}

void track_get(struct track_t *t)
{
    t->refcount++;
}

/*
 * Finish use of a track object
 */

void track_put(struct track_t *t)
{
    t->refcount--;

    if (t->refcount == 0) {
        if (t == &empty)
            return;
        assert(!t->importing);
        track_clear(t);
        free(t);
    }
}

/*
 * Get entry for use by poll()
 *
 * Return: number of file descriptors placed in *pe
 * Post: *pe contains 0 or 1 file descriptors
 */

void track_pollfd(struct track_t *t, struct pollfd *pe)
{
    assert(t->importing);

    import_pollfd(&t->import, pe);
    t->has_poll = true;
}

/*
 * Handle any file descriptor activity on this track
 *
 * Return: true if import has completed, otherwise false
 */

bool track_handle(struct track_t *tr)
{
    /* A track may be added while poll() was waiting,
     * in which case it has no return data from poll */

    if (!tr->has_poll)
        return false;

    if (import_handle(&tr->import) == -1) {
        import_stop(&tr->import);
        tr->importing = false;
        return true;
    }

    return false;
}
