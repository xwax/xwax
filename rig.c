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
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <alsa/asoundlib.h>
#include <sys/poll.h>

#include "device.h"
#include "player.h"
#include "rig.h"
#include "track.h"

#define REALTIME_PRIORITY 80
#define POLL_TIMEOUT 1000
#define MAX_POLLENT (MAX_DEVICES * 4)

int rig_init(struct rig_t *rig)
{
    int n;

    for(n = 0; n < MAX_DEVICES; n++) 
        rig->device[n] = NULL;

    for(n = 0; n < MAX_TRACKS; n++) 
        rig->track[n] = NULL;
    
    for(n = 0; n < MAX_PLAYERS; n++) 
        rig->player[n] = NULL;
    
    for(n = 0; n < MAX_TIMECODERS; n++) 
        rig->timecoder[n] = NULL;

    return 0;
}


int rig_service(struct rig_t *rig)
{
    int r, n;
    struct pollfd pt[MAX_TRACKS],
        *map[MAX_TRACKS],
        *pe;
    
    while(!rig->finished) {
        pe = pt;
        
        for(n = 0; n < MAX_TRACKS; n++) {
            if(!rig->track[n]
               || rig->track[n]->status != TRACK_STATUS_IMPORTING)
            {
                map[n] = NULL;
                continue;
            }

            pe->fd = rig->track[n]->fd;
            pe->revents = 0;
            pe->events = POLLIN | POLLHUP;
            
            map[n] = pe;

            pe++;
        }
        
        r = poll(pt, pe - pt, POLL_TIMEOUT);
        
        if(r == -1 && errno != EINTR) {
            perror("poll");
            return -1;
        }
        
        for(n = 0; n < MAX_TRACKS; n++) {
            if(!rig->track[n]
               || rig->track[n]->status != TRACK_STATUS_IMPORTING)
            {
                continue;
            }

            track_read(rig->track[n], map[n]);
        }
    }
    
    return 0;
}


int rig_realtime(struct rig_t *rig)
{
    int r, n, max_pri;
    struct sched_param sp;
    struct pollfd pt[MAX_POLLENT], *pe;
    struct device_t *dv;

    /* Setup the POSIX scheduler */
    
    fprintf(stderr, "Setting scheduler priority...\n");
    
    if(sched_getparam(0, &sp)) {
        perror("sched_getparam");
        return -1;
    }
    
    max_pri = sched_get_priority_max(SCHED_FIFO);
    sp.sched_priority = REALTIME_PRIORITY;

    if(sp.sched_priority > max_pri) {
        fprintf(stderr, "Invalid scheduling priority (maximum %d).\n",
                max_pri);
        return -1;

    }
    
    if(sched_setscheduler(0, SCHED_FIFO, &sp)) {
        perror("sched_setscheduler");
        fprintf(stderr, "Failed to set scheduler. Run as root otherwise you "
                "may get wow and skips!\n");
    }

    pe = pt;
    
    for(n = 0; n < MAX_DEVICES; n++) {
        dv = rig->device[n];
        if(!dv)
            continue;

        pe += device_fill_pollfd(dv, pe, 9999); /* FIXME */

        device_start(dv);
    }


    while(!rig->finished) {
        
        r = poll(pt, pe - pt, POLL_TIMEOUT);
        
        if(r == -1 && errno != EINTR) {
            perror("poll");
            return -1;
        }

        for(n = 0; n < MAX_DEVICES; n++) {
            if(rig->device[n]) {
                if(device_handle(rig->device[n]) == -1) 
                    return -1;
            }
        }
    }

    return 0;
}


void *service(void *p)
{
    rig_service(p);
    return p;
}


void *realtime(void *p)
{
    rig_realtime(p);
    return p;
}


int rig_start(struct rig_t *rig)
{
    rig->finished = 0;
    
    if(pthread_create(&rig->pt_service, NULL, service, (void*)rig)) {
        perror("pthread_create");
        return -1;
    }
    
    if(pthread_create(&rig->pt_realtime, NULL, realtime, (void*)rig)) {
        perror("pthread_create");
        return -1;
    }
    
    return 0;
}


int rig_stop(struct rig_t *rig)
{
    rig->finished = 1;

    if(pthread_join(rig->pt_realtime, NULL) != 0) {
        perror("pthread_join");
        return -1;
    }

    if(pthread_join(rig->pt_service, NULL) != 0) {
        perror("pthread_join");
        return -1;
    }

    return 0;
}
