/*
 * Copyright (C) 2012 Mark Hills <mark@pogo.org.uk>
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

#include "external.h"
#include "library.h"

#define CRATE_ALL "All records"

/*
 * Initialise a crate
 *
 * Note the deep copy of the crate name
 *
 * Return: 0 on success or -1 on memory allocation failure
 */

static int crate_init(struct crate *c, const char *name, bool is_fixed)
{
    c->name = strdup(name);
    if (c->name == NULL) {
        perror("strdup");
        return -1;
    }

    c->is_fixed = is_fixed;
    listing_init(&c->by_artist);
    listing_init(&c->by_bpm);

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
    listing_clear(&c->by_artist);
    listing_clear(&c->by_bpm);
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
 */

static struct record* crate_add(struct crate *c, struct record *r)
{
    struct record *x;

    assert(r != NULL);

    x = listing_insert(&c->by_artist, r, SORT_ARTIST);
    if (x != r) /* may be NULL */
        return x;

    /* FIXME: handle out-of-memory cases below */

    x = listing_insert(&c->by_bpm, r, SORT_BPM);
    assert(x == r);

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
 * Get an existing crate, or create a new one if necessary
 *
 * Return: pointer to crate, or NULL on memory allocation failure
 */

struct crate* use_crate(struct library *lib, char *name, bool is_fixed)
{
    struct crate *new_crate;

    /* does this crate already exist? then return existing crate */
    new_crate = get_crate(lib, name);
    if (new_crate != NULL) {
        fprintf(stderr, "Crate '%s' already exists...\n", name);
        return new_crate;
    }

    /* allocate and fill space for new crate */
    new_crate = malloc(sizeof(struct crate));
    if (new_crate == NULL) {
        perror("malloc");
        return NULL;
    }

    if (crate_init(new_crate, name, is_fixed) == -1)
        goto fail;

    if (add_crate(lib, new_crate) == -1)
        goto fail;

    return new_crate;

 fail:
    free(new_crate);
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

    if (crate_init(&li->all, CRATE_ALL, true) == -1)
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
    free(re->artist);
    free(re->title);
}

/*
 * Free resources associated with the music library
 */

void library_clear(struct library *li)
{
    int n;

    /* This object is responsible for all the record pointers */

    for (n = 0; n < li->all.by_artist.entries; n++) {
        struct record *re;

        re = li->all.by_artist.record[n];
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
}

/*
 * Read the next string from the file up to a valid delimeter
 *
 * Return: pointer to alloc'd string
 * Post: *delim is set
 */

static char* get_field(FILE *f, char *delim)
{
    char *s;
    size_t len;

    s = NULL;
    len = 0;

    for (;;) {
        int c;
        void *x;

        x = realloc(s, len + 1);
        if (x == NULL) {
            perror("realloc");
            free(s);
            return NULL;
        }
        s = x;

        c = fgetc(f);
        switch (c) {
        case EOF:
            *delim = '\0';
            goto done;

        case '\n':
        case '\t':
            *delim = c;
            goto done;
        }

        s[len] = c;
        len++;
    }

done:
    s[len] = '\0';
    return s;
}

/*
 * Read the next record from the file
 *
 * Return: 0 on success, otherwise -1
 * Post: if 0 is returned, *r points to an alloc'd record or NULL
 *     if EOF was found
 */

static int get_record(FILE *f, struct record **r)
{
    struct record x, *y;
    char delim, *s, *endptr;

    x.pathname = NULL;
    x.artist = NULL;
    x.title = NULL;
    x.bpm = 0.0;

    x.pathname = get_field(f, &delim);
    if (x.pathname == NULL)
        goto fail;

    /* Check for clean EOF */

    if (delim == '\0' && x.pathname[0] == '\0') {
        free(x.pathname);
        *r = NULL;
        return 0;
    }

    if (delim != '\t') {
        fprintf(stderr, "Malformed record '%s'\n", x.pathname);
        goto fail;
    }

    x.artist = get_field(f, &delim);
    if (x.artist == NULL)
        goto fail;

    if (delim != '\t') {
        fprintf(stderr, "Malformed record '%s'\n", x.pathname);
        goto fail;
    }

    x.title = get_field(f, &delim);
    if (x.title == NULL)
        goto fail;

    if (delim == '\n') /* other fields are optional */
        goto done;

    if (delim != '\t') {
        fprintf(stderr, "Malformed record '%s'\n", x.pathname);
        goto fail;
    }

    /* Beats-per-minute (BPM) */

    s = get_field(f, &delim);
    if (s == NULL)
        goto fail;

    errno = 0;
    x.bpm = strtod(s, &endptr);
    if (errno == ERANGE || *endptr != '\0'
        || !isfinite(x.bpm) || x.bpm <= 0.0)
    {
        fprintf(stderr, "%s: Ignoring malformed BPM '%s'\n", x.pathname, s);
        x.bpm = 0.0;
    }

    free(s);

    if (delim != '\n') {
        fprintf(stderr, "Malformed record '%s'\n", x.pathname);
        goto fail;
    }

done:
    y = malloc(sizeof *y);
    if (y == NULL) {
        perror("malloc");
        goto fail;
    }

    *y = x;
    *r = y;
    return 0;

fail:
    /* no-op on NULL */
    free(x.pathname);
    free(x.artist);
    free(x.title);

    return -1;
}

/*
 * Scan a record library
 *
 * Launch the given scan script and pass it the path argument.
 * Parse the results into the crates.
 *
 * Return: 0 on success, -1 on fatal error (may leak)
 */

int library_import(struct library *li, bool sort,
                   const char *scan, const char *path)
{
    int fd, status;
    char *cratename, *pathname;
    pid_t pid;
    FILE *fp;
    struct crate *crate, *all_crate;

    fprintf(stderr, "Scanning '%s'...\n", path);

    all_crate = get_crate(li, CRATE_ALL);
    assert(all_crate != NULL);

    pathname = strdupa(path);
    cratename = basename(pathname); /* POSIX version, see basename(3) */
    assert(cratename != NULL);
    crate = use_crate(li, cratename, false);
    if (crate == NULL)
        return -1;

    pid = fork_pipe(&fd, scan, "scan", path, NULL);
    if (pid == -1)
        return -1;

    fp = fdopen(fd, "r");
    if (fp == NULL) {
        perror("fdopen");
        abort(); /* recovery not implemented */
    }

    for (;;) {
        struct record *d, *x;

        if (get_record(fp, &d) == -1)
            return -1;

        if (d == NULL)
            break;

        /* Add to the crate of all records */

        x = crate_add(all_crate, d);
        if (x == NULL)
            return -1;

        /* If there is an existing entry, use it instead */

        if (x != d) {
            record_clear(d);
            free(d);
            d = x;
        }

        /* Insert into the user's crate */

        if (crate_add(crate, d) == NULL)
            return -1;
    }

    if (fclose(fp) == -1) {
        perror("close");
        abort(); /* assumption fclose() can't on read-only descriptor */
    }

    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return -1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
        fputs("Library scan exited reporting failure.\n", stderr);
        return -1;
    }

    return 0;
}
