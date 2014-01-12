/*
 * Copyright (C) 2013 Mark Hills <mark@xwax.org>
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

#define _GNU_SOURCE /* getdelim(), strdupa() */
#include <assert.h>
#include <errno.h>
#include <libgen.h> /*  basename() */
#include <math.h> /* isfinite() */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "excrate.h"
#include "external.h"

#define CRATE_ALL "All records"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))

void listing_init(struct listing *l)
{
    index_init(&l->by_artist);
    index_init(&l->by_bpm);
    index_init(&l->by_order);
    event_init(&l->addition);
}

void listing_clear(struct listing *l)
{
    index_clear(&l->by_artist);
    index_clear(&l->by_bpm);
    index_clear(&l->by_order);
    event_clear(&l->addition);
}

/*
 * Base initialiser for a crate, shared by the other init functions
 *
 * Note the deep copy of the crate name
 *
 * Return: 0 on success or -1 on memory allocation failure
 */

static int crate_init(struct crate *c, const char *name)
{
    c->name = strdup(name);
    if (c->name == NULL) {
        perror("strdup");
        return -1;
    }

    c->is_busy = false;

    event_init(&c->activity);
    event_init(&c->refresh);
    event_init(&c->addition);

    return 0;
}

/*
 * Propagate an addition event on the listing upwards -- as an
 * addition event on this crate
 */

static void propagate_addition(struct observer *o, void *x)
{
    struct crate *c = container_of(o, struct crate, on_addition);
    fire(&c->addition, x);
}

/*
 * Propagate notification that the scan has finished
 */

static void propagate_completion(struct observer *o, void *x)
{
    struct crate *c = container_of(o, struct crate, on_completion);
    c->is_busy = false;
    fire(&c->activity, NULL);
}

/*
 * Initialise the crate which shows the entire library content
 *
 * Return: 0 on success, -1 on memory allocation failure
 */

static int crate_init_all(struct library *l, struct crate *c, const char *name)
{
    if (crate_init(c, name) == -1)
        return -1;

    c->is_fixed = true;
    c->listing = &l->storage;
    watch(&c->on_addition, &c->listing->addition, propagate_addition);
    c->excrate = NULL;

    return 0;
}

/*
 * Wire in the excrate to this crate, including events
 */

static void hook_up_excrate(struct crate *c, struct excrate *e)
{
    if (!c->is_busy) {
        c->is_busy = true;
        fire(&c->activity, NULL);
    }

    c->excrate = e;
    c->listing = &e->listing;
    fire(&c->refresh, NULL);

    watch(&c->on_addition, &c->listing->addition, propagate_addition);
    watch(&c->on_completion, &e->completion, propagate_completion);
}

/*
 * Initialise a crate which has a fixed scan as its source
 *
 * Not all crates have a source (eg. 'all' crate.) This is also
 * convenient as in future there may be other sources such as virtual
 * crates or external searches.
 *
 * Return: 0 on success or -1 on error
 */

static int crate_init_scan(struct library *l, struct crate *c, const char *name,
                           const char *scan, const char *path)
{
    struct excrate *e;

    if (crate_init(c, name) == -1)
        return -1;

    c->is_fixed = false;
    c->scan = scan;
    c->path = path;

    e = excrate_acquire_by_scan(scan, path, &l->storage);
    if (e == NULL)
        return -1;

    hook_up_excrate(c, e);

    return 0;
}

/*
 * Launch a new scan operation on this crate
 *
 * Return: 0 on success, -1 on error
 */

int crate_rescan(struct library *l, struct crate *c)
{
    struct excrate *e;

    assert(c->excrate != NULL);

    /* Replace the excrate in-place. Care needed to re-wire
     * everything back up again as before */

    e = excrate_acquire_by_scan(c->scan, c->path, &l->storage);
    if (e == NULL)
        return -1;

    ignore(&c->on_completion);
    ignore(&c->on_addition);
    excrate_release(c->excrate);
    hook_up_excrate(c, e);

    return 0;
}

/*
 * Deallocate resources associated with this crate
 *
 * The crate does not 'own' the memory allocated by the records
 * in it, so we don't free them here.
 */

static void crate_clear(struct crate *c)
{
    ignore(&c->on_addition);

    if (c->excrate != NULL) {
        ignore(&c->on_completion);
        excrate_release(c->excrate);
    }

    event_clear(&c->activity);
    event_clear(&c->refresh);
    event_clear(&c->addition);
    free(c->name);
}

/*
 * Comparison function for two crates
 */

static int crate_cmp(const struct crate *a, const struct crate *b)
{
    if (a->is_fixed && !b->is_fixed)
        return -1;
    if (!a->is_fixed && b->is_fixed)
        return 1;

    return strcmp(a->name, b->name);
}

/*
 * Add a record into a crate and its various indexes
 *
 * FIXME: not all out-of-memory cases are implemented
 *
 * Return: Pointer to existing entry, NULL if out of memory
 * Post: Record added to the crate
 */

struct record* listing_add(struct listing *l, struct record *r)
{
    struct record *x;

    assert(r != NULL);

    x = index_insert(&l->by_artist, r, SORT_ARTIST);
    if (x != r) /* may be NULL */
        return x;

    x = index_insert(&l->by_bpm, r, SORT_BPM);
    if (x == NULL)
        abort(); /* FIXME: remove from all indexes and return */
    assert(x == r);

    if (index_add(&l->by_order, r) != 0)
        abort(); /* FIXME: remove from all indexes and return */

    fire(&l->addition, r);
    return r;
}

/*
 * Comparison function, see qsort(3)
 */

static int qcompar(const void *a, const void *b)
{
    return crate_cmp(*(struct crate**)a, *(struct crate**)b);
}

/*
 * Sort all crates into a defined order
 */

static void sort_crates(struct library *lib)
{
    qsort(lib->crate, lib->crates, sizeof(struct crate*), qcompar);
}

/*
 * Add a crate to the list of all crates
 *
 * Return: 0 on success or -1 on memory allocation failure
 */

static int add_crate(struct library *lib, struct crate *c)
{
    struct crate **cn;

    cn = realloc(lib->crate, sizeof(struct crate*) * (lib->crates + 1));
    if (cn == NULL) {
        perror("realloc");
        return -1;
    }

    lib->crate = cn;
    lib->crate[lib->crates++] = c;

    sort_crates(lib);

    return 0;
}

/*
 * Get a crate by the given name
 *
 * Beware: The match could match the fixed crates if the name is the
 * same.
 *
 * Return: pointer to crate, or NULL if no crate has the given name
 */

struct crate* get_crate(struct library *lib, const char *name)
{
    int n;

    for (n = 0; n < lib->crates; n++) {
        if (strcmp(lib->crate[n]->name, name) == 0)
            return lib->crate[n];
    }

    return NULL;
}

/*
 * Initialise the record library
 *
 * Return: 0 on success or -1 on memory allocation failure
 */

int library_init(struct library *li)
{
    li->crate = NULL;
    li->crates = 0;
    listing_init(&li->storage);

    if (crate_init_all(li, &li->all, CRATE_ALL) == -1)
        return -1;

    if (add_crate(li, &li->all) == -1) {
        crate_clear(&li->all);
        return -1;
    }

    return 0;
}

/*
 * Free resources associated with a record
 */

static void record_clear(struct record *re)
{
    free(re->pathname);
}

/*
 * Free resources associated with the music library
 */

void library_clear(struct library *li)
{
    int n;

    /* This object is responsible for all the record pointers */

    for (n = 0; n < li->storage.by_artist.entries; n++) {
        struct record *re;

        re = li->storage.by_artist.record[n];
        record_clear(re);
        free(re);
    }

    /* Clear crates */

    for (n = 1; n < li->crates; n++) { /* skip the 'all' crate */
        struct crate *crate;

        crate = li->crate[n];
        crate_clear(crate);
        free(crate);
    }
    free(li->crate);

    crate_clear(&li->all);
    listing_clear(&li->storage);
}

/*
 * Translate a string from the scan to our internal BPM value
 *
 * Return: internal BPM value, or INFINITY if string is malformed
 */

static double parse_bpm(const char *s)
{
    char *endptr;
    double bpm;

    if (s[0] == '\0') /* empty string, valid for 'unknown BPM' */
        return 0.0;

    errno = 0;
    bpm = strtod(s, &endptr);
    if (errno == ERANGE || *endptr != '\0' || !isfinite(bpm) || bpm <= 0.0)
        return INFINITY;

    return bpm;
}

/*
 * Split string into array of fields (destructive)
 *
 * Return: number of fields found
 * Post: array x is filled with n values
 */

static size_t split(char *s, char *x[], size_t len)
{
    size_t n;

    for (n = 0; n < len; n++) {
        char *y;

        y = strchr(s, '\t');
        if (y == NULL) {
            x[n] = s;
            return n + 1;
        }

        *y = '\0';
        x[n] = s;
        s = y + 1;
    }

    return n;
}

/*
 * Convert a line from the scan script to a record structure in memory
 *
 * Return: pointer to alloc'd record, or NULL on error
 * Post: if successful, responsibility for pointer line is taken
 */

struct record* get_record(char *line)
{
    int n;
    struct record *x;
    char *field[4];

    x = malloc(sizeof *x);
    if (!x) {
        perror("malloc");
        return NULL;
    }

    x->bpm = 0.0;

    n = split(line, field, ARRAY_SIZE(field));

    switch (n) {
    case 4:
        x->bpm = parse_bpm(field[3]);
        if (!isfinite(x->bpm)) {
            fprintf(stderr, "%s: Ignoring malformed BPM '%s'\n",
                    field[0], field[3]);
            x->bpm = 0.0;
        }
        /* fall-through */
    case 3:
        x->pathname = field[0];
        x->artist = field[1];
        x->title = field[2];
        break;

    case 2:
    case 1:
    default:
        fprintf(stderr, "Malformed record '%s'\n", line);
        goto bad;
    }

    return x;

bad:
    free(x);
    return NULL;
}

/*
 * Scan a record library
 *
 * Launch the given scan script and pass it the path argument.
 * Parse the results into the crates.
 *
 * Return: 0 on success, -1 on fatal error (may leak)
 */

int library_import(struct library *li, const char *scan, const char *path)
{
    char *cratename, *pathname;
    struct crate *crate;

    fprintf(stderr, "Adding '%s'...\n", path);

    pathname = strdupa(path);
    cratename = basename(pathname); /* POSIX version, see basename(3) */
    assert(cratename != NULL);

    crate = malloc(sizeof *crate);
    if (crate == NULL) {
        perror("malloc");
        return -1;
    }

    if (crate_init_scan(li, crate, cratename, scan, path) == -1)
        goto fail;

    if (add_crate(li, crate) == -1)
        goto fail_crate;

    return 0;

fail_crate:
    crate_clear(crate);
fail:
    free(crate);
    return -1;

}
