/*
 * Copyright (C) 2008 Mark Hills <mark@pogo.org.uk>
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

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "library.h"

#define BLOCK 256 /* number of library entries */

#define MIN(a, b) (a<b?a:b)


/* Copy src (ms characters) to dest (length md, including '\0'), and
 * ensure the result is safe and null terminated */

static int strmcpy(char *dest, int md, char *src, int ms)
{
    strncpy(dest, src, MIN(ms, md));
    if(ms < md)
        dest[ms] = '\0';
    else
        dest[md - 1] = '\0';

    return 0;
}


static int extract_info(struct record_t *lr, char *filename)
{
    char *c, *mi, *dt, *end;

    c = filename;
    end = filename;

    while(*c != '\0') {
        end = c;
        c++;
    }

    end++;

    c = filename;
    mi = end;
    
    while(c < end - 3) {
        if(c[0] == ' ' && (c[1] == '-' || c[1] == '_') && c[2] == ' ') {
            mi = c;
            break;
        }
        c++;
    }

    c = filename;
    dt = end;

    while(c < end) {
        if(*c == '.')
            dt = c;
        c++;
    }
    
    strmcpy(lr->name, MAX_NAME, filename, end - filename);
    
    if(mi == end) {
        lr->artist[0] = '\0';
        lr->title[0] = '\0';        
    } else {
        strmcpy(lr->artist, MAX_ARTIST, filename, mi - filename);
        strmcpy(lr->title, MAX_TITLE, mi + 3, dt - mi - 3);
    }        

    return 0;
}


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
    free(li->record);
}


int library_add(struct library_t *li, struct record_t *lr)
{
    struct record_t *ln;

    if(li->entries == li->size) {
        fprintf(stderr, "Allocating library space (%d entries reached)...\n",
                li->size);

        ln = realloc(li->record, sizeof(struct record_t) * li->size * 2);

        if(!ln) {
            perror("realloc");
            return -1;
        }
        
        li->record = ln;
        li->size *= 2;


    }

    memcpy(&li->record[li->entries++], lr, sizeof(struct record_t));
    
    return 0;
}


int library_import(struct library_t *li, char *path)
{
    DIR *dir;
    struct dirent *de;
    struct record_t record;
    struct stat st;
    int path_len;
    
    path_len = strlen(path);

    fprintf(stderr, "Scanning directory '%s'...\n", path);
    
    dir = opendir(path);
    if(!dir) {
        fprintf(stderr, "Failed to scan; aborting.\n");
        return -1;
    }
    
    while((de = readdir(dir)) != NULL) {

        if(de->d_name[0] == '.') /* hidden or '.' or '..' */
            continue;

        if(path_len + strlen(de->d_name) + 2 > MAX_PATHNAME) {
            fprintf(stderr, "Pathname limit exceeded; skipping '%s'.\n",
                    de->d_name);
            continue;
        }
        
        sprintf(record.pathname, "%s/%s", path, de->d_name);

        if(stat(record.pathname, &st) == -1) {
            perror("stat");
            continue;
        }

        /* If the entry is a directory, recursively import the contents
         * of the directory. Otherwise, assume a file. */

        if(S_ISDIR(st.st_mode)) {
            library_import(li, record.pathname);
       
        } else {
            extract_info(&record, de->d_name);
            library_add(li, &record);
        }
    }
    
    closedir(dir);

    return 0;
}
