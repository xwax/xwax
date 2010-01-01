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

#ifndef RIG_H
#define RIG_H

#define MAX_DEVICES 4
#define MAX_TRACKS 8
#define MAX_PLAYERS 4
#define MAX_TIMECODERS 4

#define MAX_DEVICE_POLLFDS 32

struct rig_t {
    int finished;
    pthread_t pt_realtime, pt_service;

    struct device_t *device[MAX_DEVICES];
    struct track_t *track[MAX_TRACKS];
    struct player_t *player[MAX_PLAYERS];
    struct timecoder_t *timecoder[MAX_TIMECODERS];

    int event[2]; /* pipe to wake up service thread */

    /* Poll table for devices */
    
    int npt;
    struct pollfd pt[MAX_DEVICE_POLLFDS];
};

int rig_init(struct rig_t *rig);
int rig_start(struct rig_t *rig);
int rig_awaken(struct rig_t *rig);
int rig_stop(struct rig_t *rig);

#endif
