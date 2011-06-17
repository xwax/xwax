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

#ifndef DEVICE_H
#define DEVICE_H

#include <sys/poll.h>
#include <sys/types.h>

#define DEVICE_CHANNELS 2

struct device {
    void *local;
    struct device_ops *ops;

    struct timecoder_t *timecoder;
    struct player_t *player;
};

struct device_ops {
    ssize_t (*pollfds)(struct device *dv, struct pollfd *pe, size_t z);
    int (*handle)(struct device *dv);

    unsigned int (*sample_rate)(struct device *dv);
    void (*start)(struct device *dv);
    void (*stop)(struct device *dv);

    void (*clear)(struct device *dv);
};

void device_connect_timecoder(struct device *dv, struct timecoder_t *tc);
void device_connect_player(struct device *dv, struct player_t *pl);

unsigned int device_sample_rate(struct device *dv);

void device_start(struct device *dv);
void device_stop(struct device *dv);

void device_clear(struct device *dv);

ssize_t device_pollfds(struct device *dv, struct pollfd *pe, size_t z);
int device_handle(struct device *dv);

#endif
