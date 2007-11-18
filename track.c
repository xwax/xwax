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

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "track.h"

#define SAMPLE 4 /* bytes per sample (all channels) */


void track_init(struct track_t *tr, const char *importer)
{
    tr->importer = importer;
    
    tr->bytes = 0;
    tr->length = 0;
    tr->blocks = 0;
    
    tr->pid = 0;
    
    tr->artist = NULL;
    tr->title = NULL;
    tr->name = NULL;
        
    tr->status = TRACK_STATUS_CLEAR;
}


void track_clear(struct track_t *tr)
{
    int n;
    
    for(n = 0; n < tr->blocks; n++)
        free(tr->block[n]);
}


int track_import(struct track_t *tr, char *path)
{
    int pstdout[2];
    
    if(pipe(pstdout) == -1) {
        perror("pipe");
        return -1;
    }
    
    tr->pid = vfork();
    
    if(tr->pid < 0) {
        perror("vfork");
        return -1;
        
    } else if(tr->pid == 0) { /* child */

        close(pstdout[0]);
        dup2(pstdout[1], STDOUT_FILENO);
        close(pstdout[1]);
        
        if(execl(tr->importer, "import", path, NULL) == -1) {
            perror("execl");
            exit(-1);
            return 0;
        }

    } else { /* parent */
        
        close(pstdout[1]);
        
        tr->bytes = 0;
        tr->length = 0;
        tr->ppm = 0;
        tr->overview = 0;
        
        tr->fd = pstdout[0];
        
        if(fcntl(tr->fd, F_SETFL, O_NONBLOCK) == -1) {
            perror("fcntl");
            return -1;
        }
        
        tr->status = TRACK_STATUS_IMPORTING;
    }
    
    /* The conversion process has been forked, so now make repeated
     * calls track_read() to import the data */
    
    return 0;
}


/* Read the next block of data from the file */

int track_read(struct track_t *tr, struct pollfd *pe)
{
    int r, ls, used;
    size_t m;
    unsigned short v;
    unsigned int s, w;
    struct track_block_t *block;
    
    if(pe && !pe->revents)
        return 0;

    /* Check whether we need to allocate a new block */
    
    if(tr->bytes >= (size_t)tr->blocks * TRACK_BLOCK_SAMPLES * SAMPLE) {

        if(tr->blocks >= TRACK_MAX_BLOCKS) {
            fprintf(stderr, "Maximum track length reached; aborting...\n");
            track_abort(tr);
            return 0;
        }
                
        block = malloc(sizeof(struct track_block_t));
        if(!block) {
            perror("malloc");
            return -1;
        }

        tr->block[tr->blocks++] = block;

        fprintf(stderr, "Allocated new track block (%d at %zu bytes).\n",
                tr->blocks, tr->bytes);
    }

    /* Load in audio to the end of the current block. We've just
     * allocated a new one if needed, so no possibility of read()
     * returning zero, except for EOF */

    block = tr->block[tr->bytes / (TRACK_BLOCK_SAMPLES * SAMPLE)];
    used = tr->bytes % (TRACK_BLOCK_SAMPLES * SAMPLE);
    
    r = read(tr->fd, (char*)block->pcm + used,
             TRACK_BLOCK_SAMPLES * SAMPLE - used);
    
    if(r == 0) {
        fprintf(stderr, "Importing track complete.\n");
        
        m = TRACK_BLOCK_SAMPLES * SAMPLE * tr->blocks / 1024;
        
        fprintf(stderr, "Track memory %zuKb PCM, %zuKb PPM, %zuKb overview.\n",
                m, m / TRACK_PPM_RES, m / TRACK_OVERVIEW_RES);
        
        if(close(tr->fd) == -1) {
            perror("close");
            tr->status = TRACK_STATUS_UNKNOWN;
            return -1;
        }
        
        tr->status = TRACK_STATUS_WAITING;
        
        return 0;
        
    } else if(r == -1) {
        if(errno == EAGAIN)
            return 0;
        
        perror("read");
        tr->status = TRACK_STATUS_UNKNOWN;
        return -1;
    }
    
    tr->bytes += r;

    /* Meter the audio which has just been read */
    
    for(s = tr->length; s < tr->bytes / SAMPLE; s++) {
        ls = s % TRACK_BLOCK_SAMPLES;
        
        v = (abs(block->pcm[ls * TRACK_CHANNELS])
             + abs(block->pcm[ls * TRACK_CHANNELS]));
        
        /* PPM-style fast meter approximation */

        if(v > tr->ppm)
            tr->ppm += (v - tr->ppm) >> 3;
        else
            tr->ppm -= (tr->ppm - v) >> 9;
        
        if(ls % TRACK_PPM_RES == TRACK_PPM_RES - 1)
            block->ppm[ls / TRACK_PPM_RES] = tr->ppm >> 8;
        
        /* Update the slow-metering overview. Fixed point arithmetic
         * going on here */

        w = v << 16;
        
        if(w > tr->overview)
            tr->overview += (w - tr->overview) >> 8;
        else
            tr->overview -= (tr->overview - w) >> 17;

        if(ls % TRACK_OVERVIEW_RES == TRACK_PPM_RES - 1)
            block->overview[ls / TRACK_OVERVIEW_RES] = tr->overview >> 24;
    }
    
    tr->length = s;

    return r;
}


/* Must wait for the forked application and allow it to terminate */

int track_wait(struct track_t *tr)
{
    int status;

    if(tr->status != TRACK_STATUS_WAITING)
        return 0;

    fprintf(stderr, "Entering track wait...\n");

    if(waitpid(tr->pid, &status, 0) == -1) {
        perror("waitpid");
        return -1;
    }

    fprintf(stderr, "Track importer exited with status %d.\n", status);

    tr->pid = 0;
    tr->status = TRACK_STATUS_COMPLETE;

    return 0;
}


int track_abort(struct track_t *tr)
{
    if(tr->status == TRACK_STATUS_IMPORTING) {
        
        fprintf(stderr, "Aborting track import...\n");

        if(close(tr->fd) == -1) {
            perror("close");
            return -1;
        }
        
        if(kill(tr->pid, SIGTERM) == -1) {
            perror("kill");
            return -1;
        }
        
        tr->status = TRACK_STATUS_WAITING;
    }

    return 0;
}
