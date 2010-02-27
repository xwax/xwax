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

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/poll.h>

#include "device.h"
#include "player.h"
#include "rig.h"
#include "track.h"

#define REALTIME_PRIORITY 80


int rig_init(struct rig_t *rig)
{
    int n;

    for (n = 0; n < MAX_DEVICES; n++)
        rig->device[n] = NULL;

    for (n = 0; n < MAX_TRACKS; n++)
        rig->track[n] = NULL;
    
    for (n = 0; n < MAX_PLAYERS; n++)
        rig->player[n] = NULL;
    
    for (n = 0; n < MAX_TIMECODERS; n++)
        rig->timecoder[n] = NULL;

    return 0;
}


int rig_service(struct rig_t *rig)
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
        
        if (r == -1 && errno != EINTR) {
            perror("poll");
            return -1;
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


int rig_realtime(struct rig_t *rig)
{
    int r, n, max_pri;
    struct sched_param sp;

    /* Setup the POSIX scheduler */
    
    fprintf(stderr, "Setting scheduler priority...\n");
    
    if (sched_getparam(0, &sp)) {
        perror("sched_getparam");
        return -1;
    }
    
    max_pri = sched_get_priority_max(SCHED_FIFO);
    sp.sched_priority = REALTIME_PRIORITY;

    if (sp.sched_priority > max_pri) {
        fprintf(stderr, "Invalid scheduling priority (maximum %d).\n",
                max_pri);
        return -1;

    }
    
    if (sched_setscheduler(0, SCHED_FIFO, &sp)) {
        perror("sched_setscheduler");
        fprintf(stderr, "Failed to set scheduler. Run as root otherwise you "
                "may get wow and skips!\n");
    }

    while (!rig->finished) {
        r = poll(rig->pt, rig->npt, -1);
        
        if (r == -1 && errno != EINTR) {
            perror("poll");
            return -1;
        }

        for (n = 0; n < MAX_DEVICES; n++) {
            if (rig->device[n])
                device_handle(rig->device[n]);
        }
    }

    return 0;
}


void* service(void *p)
{
    rig_service(p);
    return p;
}


void* realtime(void *p)
{
    rig_realtime(p);
    return p;
}


int rig_start(struct rig_t *rig)
{
    int r, n;
    struct pollfd *pe, *pm;
    struct device_t *dv;

    rig->finished = 0;

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

    if (pthread_create(&rig->pt_service, NULL, service, (void*)rig)) {
        perror("pthread_create");
        return -1;
    }

    /* The requested poll events never change, so populate the poll
     * entry table before entering the realtime thread */

    pe = rig->pt;
    pm = rig->pt + MAX_DEVICE_POLLFDS;

    for (n = 0; n < MAX_DEVICES; n++) {
        dv = rig->device[n];
        
        if (!dv)
            continue;
        
        r = device_pollfds(dv, pe, pm - pe);
        if (r == -1) {
            fprintf(stderr, "Device failed to return file descriptors.\n");
            return -1;
        }

        pe += r;

        /* Start the audio rolling on the device */

        if (device_start(dv) == -1)
            return -1;
    }

    rig->npt = pe - rig->pt;

    /* If there are any devices which returned file descriptors for
     * poll() then launch the realtime thread to handle them */

    if (rig->npt > 0) {
        fprintf(stderr, "Launching realtime thread to handle devices...\n");

        if (pthread_create(&rig->pt_realtime, NULL, realtime, (void*)rig)) {
            perror("pthread_create");
            return -1;
        }
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
    int n;

    rig->finished = 1;

    if (rig->npt > 0) {
        if (pthread_join(rig->pt_realtime, NULL) != 0) {
            perror("pthread_join");
            return -1;
        }
    }

    /* Stop audio rolling on devices */

    for (n = 0; n < MAX_DEVICES; n++) {
        if (rig->device[n])
            device_stop(rig->device[n]);
    }

    rig_awaken(rig);

    if (pthread_join(rig->pt_service, NULL) != 0) {
        perror("pthread_join");
        return -1;
    }

    return 0;
}
