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

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/poll.h>

#include "rig.h"
#include "track.h"


int rig_init(struct rig_t *rig)
{
    int n;

    for (n = 0; n < MAX_TRACKS; n++)
        rig->track[n] = NULL;

    return 0;
}


static int rig_service(struct rig_t *rig)
{
    int r, n;
    char buf;
    struct pollfd pt[MAX_TRACKS], *pe;
    
    while (!rig->finished) {
        pe = pt;

        /* ppoll() is not widely available, so use a pipe between this
         * thread and the outside. A single byte wakes up poll() to
         * inform us that new file descriptors need to be polled */

        pe->fd = rig->event[0];
        pe->revents = 0;
        pe->events = POLLIN | POLLHUP | POLLERR;
        pe++;

        /* Fetch file descriptors to monitor from each track */

        for (n = 0; n < MAX_TRACKS; n++) {
            if (rig->track[n])
                pe += track_pollfd(rig->track[n], pe);
        }

        r = poll(pt, pe - pt, -1);
        if (r == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                perror("poll");
                return -1;
            }
        }

        /* If we were awakened, take the top byte off the event pipe */

        if (pt[0].revents) {
            if (read(rig->event[0], &buf, 1) == -1) {
                perror("read");
                return -1;
            }
        }

        /* Do any reading and writing on all tracks */

        for (n = 0; n < MAX_TRACKS; n++) {
            if (rig->track[n])
                track_handle(rig->track[n]);
        }
    }
    
    return 0;
}


static void* service(void *p)
{
    rig_service(p);
    return p;
}


int rig_start(struct rig_t *rig)
{
    int n;

    rig->finished = false;

    /* Register ourselves with the tracks we are looking after */

    for (n = 0; n < MAX_TRACKS; n++) {
        if (rig->track[n])
            rig->track[n]->rig = rig;
    }

    /* Create a pipe which will be used to wake the service thread */

    if (pipe(rig->event) == -1) {
        perror("pipe");
        return -1;
    }

    if (pthread_create(&rig->ph, NULL, service, (void*)rig)) {
        perror("pthread_create");
        return -1;
    }

    return 0;
}


/* Wake up the rig when a track controlled by it has changed */

int rig_awaken(struct rig_t *rig)
{
    /* Send a single byte down the event pipe to a waiting poll() */

    if (write(rig->event[1], "\0", 1) == -1) {
        perror("write");
        return -1;
    }

    return 0;
}


int rig_stop(struct rig_t *rig)
{
    rig->finished = true;
    rig_awaken(rig);

    if (pthread_join(rig->ph, NULL) != 0) {
        perror("pthread_join");
        return -1;
    }

    return 0;
}
