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
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "library.h"

#define BLOCK 256 /* number of library entries */


int library_init(struct library_t *li)
{
    li->record = malloc(sizeof(struct record_t) * BLOCK);
    if(li->record == NULL) {
        perror("malloc");
        return -1;
    }

    li->size = BLOCK;
    li->entries = 0;

    return 0;
}


void library_clear(struct library_t *li)
{
    int n;

    for(n = 0; n < li->entries; n++) {
        free(li->record[n].pathname);
        free(li->record[n].artist);
        free(li->record[n].title);
    }

    free(li->record);
}


int library_add(struct library_t *li, struct record_t *lr)
{
    struct record_t *ln;

    if(li->entries == li->size) {
        fprintf(stderr, "Allocating library space (%d entries reached)...\n",
                li->size);

        ln = realloc(li->record, sizeof(struct record_t) * li->size * 2);
        if(ln == NULL) {
            perror("realloc");
            return -1;
        }

        li->record = ln;
        li->size *= 2;
    }

    li->record[li->entries++] = *lr;

    return 0;
}


/* Read and allocate a null-terminated field from the given file handle.
 * If the empty string is read, *s is set to NULL. Return 0 on success,
 * -1 on error or EOF */

static int get_field(FILE *fp, char **f)
{
    char *s = NULL;
    size_t n;

    if(getdelim(&s, &n, '\0', fp) == -1) {
        free(s);
        return -1;
    }

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
        struct record_t d;

        if(get_field(fp, &d.pathname) != 0)
            break;

        if(get_field(fp, &d.artist) != 0) {
            fprintf(stderr, "EOF when reading artist for '%s'.\n", d.artist);
            return -1;
        }

        if(get_field(fp, &d.title) != 0) {
            fprintf(stderr, "EOF when reading title for '%s'.\n", d.title);
            return -1;
        }

        if(library_add(li, &d) == -1)
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
