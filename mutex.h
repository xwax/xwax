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
 * Mutex locking for syncronisation between low priority threads
 */

#ifndef MUTEX_H
#define MUTEX_H

#include "realtime.h"

typedef pthread_mutex_t mutex;

static inline void mutex_init(mutex *m)
{
    if (pthread_mutex_init(m, NULL) != 0)
        abort();
}

/*
 * Pre: lock is not held
 */

static inline void mutex_clear(mutex *m)
{
    int r;

    r = pthread_mutex_destroy(m);
    if (r != 0) {
        errno = r;
        perror("pthread_mutex_destroy");
        abort();
    }
}

/*
 * Take a mutex lock
 *
 * Pre: lock is initialised
 * Pre: lock is not held by this thread
 * Post: lock is held by this thread
 */

static inline void mutex_lock(mutex *m)
{
    rt_not_allowed();

    if (pthread_mutex_lock(m) != 0)
        abort();
}

/*
 * Release a mutex lock
 *
 * Pre: lock is held by this thread
 * Post: lock is not held
 */

static inline void mutex_unlock(mutex *m)
{
    if (pthread_mutex_unlock(m) != 0)
        abort();
}

#endif
