/*
 * Copyright (C) 2009 Mark Hills <mark@pogo.org.uk>
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

#define _GNU_SOURCE /* getdelim() */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "library.h"


int library_init(struct library_t *li)
{
    if(listing_init(&li->storage) != 0)
        return -1;

    return 0;
}


static void record_clear(struct record_t *re)
{
    free(re->pathname);
    free(re->artist);
    free(re->title);
}


void library_clear(struct library_t *li)
{
    int n;

    /* This object owns all the record pointers */

    for(n = 0; n < li->storage.entries; n++) {
        struct record_t *re;

        re = li->storage.record[n];
        record_clear(re);
        free(re);
    }

    listing_clear(&li->storage);
}


/* Read and allocate a null-terminated field from the given file handle.
 * If the empty string is read, *s is set to NULL. Return 0 on success,
 * -1 on error or EOF */

static int get_field(FILE *fp, char delim, char **f)
{
    char *s = NULL;
    size_t n, z;

    z = getdelim(&s, &n, delim, fp);
    if(z == -1) {
        free(s);
        return -1;
    }

    s[z - 1] = '\0'; /* don't include delimiter */
    *f = s;
    return 0;
}


/* Scan a record library at the given path, using the given scan
 * script. Returns -1 on fatal error which may leak resources */

int library_import(struct library_t *li, const char *scan, const char *path)
{
    int pstdout[2], status;
    pid_t pid;
    FILE *fp;

    fprintf(stderr, "Scanning '%s'...\n", path);

    if(pipe(pstdout) == -1) {
        perror("pipe");
        return -1;
    }

    pid = vfork();

    if(pid == -1) {
        perror("vfork");
        return -1;

    } else if(pid == 0) { /* child */

        if(close(pstdout[0]) == -1) {
            perror("close");
            abort();
        }

        if(dup2(pstdout[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            abort();
        }

        if(close(pstdout[1]) == -1) {
            perror("close");
            abort();
        }

        if(execl(scan, "scan", path, NULL) == -1) {
            perror("execl");
            exit(-1);
        }

        abort(); /* execl() does not return */
    }

    if(close(pstdout[1]) == -1) {
        perror("close");
        abort();
    }

    fp = fdopen(pstdout[0], "r");
    if(fp == NULL) {
        perror("fdopen");
        return -1;
    }

    for(;;) {
        struct record_t *d;

        d = malloc(sizeof(struct record_t));
        if (d == NULL) {
            perror("malloc");
            return -1;
        }

        if(get_field(fp, '\t', &d->pathname) != 0)
            break;

        if(get_field(fp, '\t', &d->artist) != 0) {
            fprintf(stderr, "EOF when reading artist for '%s'.\n", d->pathname);
            return -1;
        }

        if(get_field(fp, '\n', &d->title) != 0) {
            fprintf(stderr, "EOF when reading title for '%s'.\n", d->pathname);
            return -1;
        }

        if(listing_add(&li->storage, d) != 0)
            return -1;
    }

    if(fclose(fp) == -1) {
        perror("close");
        abort(); /* assumption fclose() can't on read-only descriptor */
    }

    if(waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return -1;
    }

    if(!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
        fputs("Library scan exited reporting failure.\n", stderr);
        return -1;
    }

    return 0;
}
