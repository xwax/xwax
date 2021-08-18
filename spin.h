/*
 * Copyright (C) 2021 Mark Hills <mark@xwax.org>
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
 * Spinlock routines for synchronising with the realtime thread
 */

#ifndef SPIN_H
#define SPIN_H

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "realtime.h"

typedef pthread_spinlock_t spin;

static inline void spin_init(spin *s)
{
    if (pthread_spin_init(s, PTHREAD_PROCESS_PRIVATE) != 0)
        abort();
}

static inline void spin_clear(spin *s)
{
    if (pthread_spin_destroy(s) != 0)
        abort();
}

/*
 * Take a spinlock
 *
 * Pre: lock is initialised
 * Pre: lock is not held by the current thread
 * Pre: current thread is not realtime
 * Post: lock is held by the current thread
 */

static inline void spin_lock(spin *s)
{
    rt_not_allowed();

    if (pthread_spin_lock(s) != 0)
        abort();
}

/*
 * Try and take a spinlock
 *
 * Pre: lock is initialised
 * Pre: lock is not held by the current thread
 * Post: if true is returned, lock is held by the current thread
 */

static inline bool spin_try_lock(spin *s)
{
    int r;

    r = pthread_spin_trylock(s);
    switch (r) {
    case 0:
        return true;
    case EBUSY:
        return false;
    default:
        abort();
    }
}

/*
 * Release a spinlock
 *
 * Pre: lock is held by this thread
 * Post: lock is not held
 */

static inline void spin_unlock(spin *s)
{
    if (pthread_spin_unlock(s) != 0)
        abort();
}

#endif

