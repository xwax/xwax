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

#ifndef REALTIME_H
#define REALTIME_H

#include <stdbool.h>

#define MAX_DEVICES 4
#define MAX_DEVICE_POLLFDS 32

/*
 * State data for the realtime thread, maintained during rt_start and
 * rt_stop
 */

struct rt_t {
    pthread_t ph;
    bool finished;

    size_t ndv;
    struct device_t *dv[MAX_DEVICES];

    size_t npt;
    struct pollfd pt[MAX_DEVICE_POLLFDS];
};

int rt_start(struct rt_t *rt, struct device_t *dv, size_t ndv);
void rt_stop(struct rt_t *rt);

#endif
