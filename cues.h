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

#ifndef CUES_H
#define CUES_H

#include <math.h>

#define MAX_CUES 16
#define CUE_UNSET (HUGE_VAL)

/*
 * A set of cue points
 */

struct cues {
    double position[MAX_CUES];
};

void cues_reset(struct cues *q);

void cues_unset(struct cues *q, unsigned int label);
void cues_set(struct cues *q, unsigned int label, double position);
double cues_get(const struct cues *q, unsigned int label);
double cues_prev(const struct cues *q, double current);
double cues_next(const struct cues *q, double current);

#endif
