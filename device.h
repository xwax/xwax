/*
 * Copyright (C) 2025 Mark Hills <mark@xwax.org>
 *
 * This file is part of "xwax".
 *
 * "xwax" is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * "xwax" is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef DEVICE_H
#define DEVICE_H

#include <poll.h>
#include <stdbool.h>
#include <sys/types.h>

#define DEVICE_CHANNELS 2

struct device {
    bool fault;
    void *local;
    struct device_ops *ops;

    struct timecoder *timecoder;
    struct player *player;
};

struct device_ops {
    ssize_t (*pollfds)(struct device *dv, struct pollfd *pe, size_t z);
    int (*handle)(struct device *dv);

    unsigned int (*sample_rate)(struct device *dv);
    void (*start)(struct device *dv);
    void (*stop)(struct device *dv);

    void (*clear)(struct device *dv);
};

void device_init(struct device *dv, struct device_ops *ops);
void device_clear(struct device *dv);

void device_connect_timecoder(struct device *dv, struct timecoder *tc);
void device_connect_player(struct device *dv, struct player *pl);

unsigned int device_sample_rate(struct device *dv);

void device_start(struct device *dv);
void device_stop(struct device *dv);

ssize_t device_pollfds(struct device *dv, struct pollfd *pe, size_t z);
void device_handle(struct device *dv);

void device_submit(struct device *dv, signed short *pcm, size_t npcm);
void device_collect(struct device *dv, signed short *pcm, size_t npcm);

#endif
