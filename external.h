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

/*
 * Utility functions for launching external processes
 */

#ifndef EXTERNAL_H
#define EXTERNAL_H

#include <stdarg.h>
#include <unistd.h>

/*
 * A handy read buffer; an equivalent of fread() but for
 * non-blocking file descriptors
 */

struct rb {
    char buf[4096];
    size_t len;
};

pid_t fork_pipe(int *fd, const char *path, char *arg, ...);
pid_t fork_pipe_nb(int *fd, const char *path, char *arg, ...);

void rb_reset(struct rb *rb);
ssize_t get_line(int fd, struct rb *rb, char **string);

#endif
