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

#ifndef IMPORT_H
#define IMPORT_H

#include <sys/types.h>

/*
 * State information for an import operation -- data is piped from
 * an external process into a track
 */

struct import_t {
    int fd;
    pid_t pid;
    struct pollfd *pe;
    struct track_t *track;
};

int import_start(struct import_t *im, struct track_t *track,
                 const char *cmd, const char *path);
void import_pollfd(struct import_t *im, struct pollfd *pe);
int import_handle(struct import_t *im);
void import_terminate(struct import_t *im);
void import_stop(const struct import_t *im);

#endif
