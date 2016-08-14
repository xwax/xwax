/*
 * Copyright (C) 2016 Mark Hills <mark@xwax.org>
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

/*
 * External record library ('excrate')
 *
 * Implement search in an external script. The results are streamed
 * back into a local listing.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "debug.h"
#include "excrate.h"
#include "rig.h"
#include "status.h"

static struct list excrates = LIST_INIT(excrates);

static int excrate_init(struct excrate *e, const char *script,
                        const char *search, struct listing *storage)
{
    pid_t pid;

    fprintf(stderr, "External scan '%s'...\n", search);

    pid = fork_pipe_nb(&e->fd, script, "scan", search, NULL);
    if (pid == -1)
        return -1;

    e->pid = pid;
    e->pe = NULL;
    e->terminated = false;
    e->refcount = 0;
    rb_reset(&e->rb);
    listing_init(&e->listing);
    e->storage = storage;
    event_init(&e->completion);
    e->search = search;

    list_add(&e->excrates, &excrates);
    rig_post_excrate(e);

    return 0;
}

static void excrate_clear(struct excrate *e)
{
    assert(e->pid == 0);
    list_del(&e->excrates);
    listing_clear(&e->listing);
    event_clear(&e->completion);
}

struct excrate* excrate_acquire_by_scan(const char *script, const char *search,
                                        struct listing *storage)
{
    struct excrate *e;

    debug("get_by_scan %s, %s", script, search);

    e = malloc(sizeof *e);
    if (e == NULL) {
        perror("malloc");
        return NULL;
    }

    if (excrate_init(e, script, search, storage) == -1) {
        free(e);
        return NULL;
    }

    excrate_acquire(e);

    debug("returning %p", e)
    return e;
}

void excrate_acquire(struct excrate *e)
{
    debug("get %p", e);
    e->refcount++;
}

/*
 * Request premature termination of the scan
 */

static void terminate(struct excrate *e)
{
    assert(e->pid != 0);
    debug("terminating %d", e->pid);

    if (kill(e->pid, SIGTERM) == -1)
        abort();

    e->terminated = true;
}

void excrate_release(struct excrate *e)
{
    debug("put %p, refcount=%d", e, e->refcount);
    e->refcount--;

    /* Scan must terminate before this object goes away */

    if (e->refcount == 1 && e->pid != 0) {
        debug("%p still executing but not longer required", e);
        terminate(e);
        return;
    }

    if (e->refcount == 0) {
        excrate_clear(e);
        free(e);
    }
}

/*
 * Get entry for use by poll()
 *
 * Pre: scan is running
 * Post: *pe contains poll entry
 */

void excrate_pollfd(struct excrate *e, struct pollfd *pe)
{
    assert(e->pid != 0);

    pe->fd = e->fd;
    pe->events = POLLIN;

    e->pe = pe;
}

static void do_wait(struct excrate *e)
{
    int status;

    assert(e->pid != 0);
    debug("waiting on pid %d", e->pid);

    if (close(e->fd) == -1)
        abort();

    if (waitpid(e->pid, &status, 0) == -1)
        abort();

    debug("wait for pid %d returned %d", e->pid, status);

    if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
        fprintf(stderr, "Scan completed\n");
    } else {
        fprintf(stderr, "Scan completed with status %d\n", status);
        if (!e->terminated)
            status_printf(STATUS_ALERT, "Error scanning %s", e->search);
    }

    e->pid = 0;
}

/*
 * Return: -1 on completion, otherwise zero
 */

static int read_from_pipe(struct excrate *e)
{
    for (;;) {
        char *line;
        ssize_t z;
        struct record *d, *x;

        z = get_line(e->fd, &e->rb, &line);
        if (z == -1) {
            if (errno == EAGAIN)
                return 0;
            perror("get_line");
            return -1;
        }

        if (z == 0)
            return -1;

        debug("got line '%s'", line);

        d = get_record(line);
        if (d == NULL) {
            free(line);
            continue; /* ignore malformed entries */
        }

        x = listing_add(e->storage, d);
        if (x == NULL)
            return -1;
        if (x != d) /* our new record is a duplicate */
            free(d);

        x = listing_add(&e->listing, x);
        if (x == NULL)
            return -1;
    }
}

void excrate_handle(struct excrate *e)
{
    assert(e->pid != 0);

    if (e->pe == NULL)
        return;

    if (e->pe->revents == 0)
        return;

    if (read_from_pipe(e) != -1)
        return;

    do_wait(e);
    fire(&e->completion, NULL);
    list_del(&e->rig);
    excrate_release(e); /* may invalidate e */
}
