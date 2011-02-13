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

#ifndef CUES_H
#define CUES_H

#include <math.h>

#define MAX_CUES 16
#define CUE_UNSET (HUGE_VAL)

/*
 * A set of cue points
 */

struct cues_t {
    double position[MAX_CUES];
};

void cues_reset(struct cues_t *q);

void cues_unset(struct cues_t *q, unsigned int label);
void cues_set(struct cues_t *q, unsigned int label, double position);
double cues_get(const struct cues_t *q, unsigned int label);
double cues_prev(const struct cues_t *q, double current);
double cues_next(const struct cues_t *q, double current);

#endif
