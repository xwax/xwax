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
 * Implement a global one-line status console
 */

#ifndef STATUS_H
#define STATUS_H

#include <stdarg.h>

#include "observer.h"

#define STATUS_VERBOSE 0
#define STATUS_INFO    1
#define STATUS_WARN    2
#define STATUS_ALERT   3

extern struct event status_changed;

const char* status(void);
int status_level(void);

void status_set(int level, const char *s);
void status_printf(int level, const char *s, ...);

#endif
