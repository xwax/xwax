/*
 * Copyright (C) 2024 Mark Hills <mark@xwax.org>
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

#include <assert.h>
#include <stdio.h>

#include "cues.h"

/*
 * Self-contained test of cue points
 */

int main(int argc, char *argv[])
{
    struct cues q;

    cues_reset(&q);

    assert(cues_get(&q, 0) == CUE_UNSET);

    cues_set(&q, 0, 100.0);
    assert(cues_get(&q, 0) == 100.0);

    return 0;
}
