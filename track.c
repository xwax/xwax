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

#define LOCK(tr) pthread_mutex_lock(&(tr)->mx)
#define UNLOCK(tr) pthread_mutex_unlock(&(tr)->mx)

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
 * Initialise object which will hold PCM audio data
 *
 * Post: track is initialised
 */

void track_init(struct track_t *tr, const char *importer)
{
    tr->importer = importer;
    tr->artist = NULL;
    tr->title = NULL;

    tr->blocks = 0;
    tr->importing = false;
    tr->rate = TRACK_RATE;

    track_empty(tr);

    if (pthread_mutex_init(&tr->mx, NULL) != 0)
        abort();
}

/*
 * Destroy this track from memory
 *
 * Terminates any import processes and frees any memory allocated by
 * this object.
 *
 * Pre: track is initialised
 */

void track_clear(struct track_t *tr)
{
    int n;

    /* Force a cleanup of whichever state we are in */

    if (tr->importing) {
        import_terminate(&tr->import);
        import_stop(&tr->import);
    }

    for (n = 0; n < tr->blocks; n++)
        free(tr->block[n]);

    if (pthread_mutex_destroy(&tr->mx) != 0)
        abort();
}

/*
 * Make the given track empty
 *
 * This function does not deallocate the buffer, which is a quick
 * way to handle concurrency issues -- the realtime thread accessing
 * the audio.
 *
 * Post: track is 0 seconds in length
 */

void track_empty(struct track_t *tr)
{
    tr->bytes = 0;
    tr->length = 0;
    tr->ppm = 0;
    tr->overview = 0;
}

/*
 * Import audio from the given path
 *
 * Cleans up any existing import operation, and so can be called
 * when the track is in any initialised state.
 *
 * Post: track is importing
 */

int track_import(struct track_t *tr, const char *path)
{
    int r;

    LOCK(tr);

    /* Abort any running import process */

    if (tr->importing) {
        import_terminate(&tr->import);
        import_stop(&tr->import);
    }

    /* Start the new import process */

    r = import_start(&tr->import, tr, tr->importer, path);

    UNLOCK(tr);

    return r;
}

/*
 * Get entry for use by poll()
 *
 * Return: number of file descriptors placed in *pe
 * Post: *pe contains 0 or 1 file descriptors
 */

size_t track_pollfd(struct track_t *tr, struct pollfd *pe)
{
    int r;

    LOCK(tr);

    if (tr->importing) {
        import_pollfd(&tr->import, pe);
        tr->has_poll = true;
        r = 1;
    } else {
        tr->has_poll = false;
        r = 0;
    }

    UNLOCK(tr);

    return r;
}

/*
 * Handle any file descriptor activity on this track
 */

void track_handle(struct track_t *tr)
{
    LOCK(tr);

    /* Number of file descriptors watched will fluctuate between zero
     * and non-zero, so we need to use has_poll */

    if (tr->importing && tr->has_poll) {
        if (import_handle(&tr->import) == -1) {
            import_stop(&tr->import);
            tr->importing = false;
        }
    }

    UNLOCK(tr);
}
