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

#include <poll.h>
#include <stdbool.h>

/*
 * State data for the realtime thread, maintained during rt_start and
 * rt_stop
 */

struct rt_t {
    pthread_t ph;
    bool finished;

    size_t ndv;
    struct device *dv[3];

    size_t npt;
    struct pollfd pt[32];
};

void rt_init(struct rt_t *rt);
void rt_clear(struct rt_t *rt);

int rt_add_device(struct rt_t *rt, struct device *dv);

int rt_start(struct rt_t *rt);
void rt_stop(struct rt_t *rt);

#endif
