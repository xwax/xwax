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

#ifndef DEVICE_H
#define DEVICE_H

#include <sys/poll.h>

#define DEVICE_CHANNELS 2
#define DEVICE_RATE 44100
#define DEVICE_FRAME 32

struct device_t {
    int fd;
    struct pollfd *pe;

    struct timecoder_t *timecoder;
    struct player_t *player;
};

int device_open(struct device_t *dv, const char *filename,
                unsigned short buffers, unsigned short fragment);
int device_close(struct device_t *dv);
int device_handle(struct device_t *dv);

#endif
