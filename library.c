/*
 * Copyright (C) 2007 Mark Hills <mark@pogo.org.uk>
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

#include <ctype.h>
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

static int strmatch(char *little, char *big)
{
    size_t n, m, match, little_len;

    little_len = strlen(little);

    for(n = 0; n + little_len <= strlen(big); n++) {
        
        match = 1;

        for(m = 0; m < little_len; m++) {
            if(tolower(little[m]) != tolower(big[n + m])) {
                match = 0;
                break;
            }
        }

        if(match)
            return 1;
    }

    return 0;
}


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


int library_clear(struct library_t *li)
{
    if(li->record)
        free(li->record);

    return 0;
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


int library_get_listing(struct library_t *li, struct listing_t *ls)
{
    int n;

    for(n = 0; n < li->entries; n++)
        listing_add(ls, &li->record[n]);
   
    return 0;
}


int listing_init(struct listing_t *ls)
{
    ls->record = malloc(sizeof(struct record_t*) * BLOCK);
    if(ls->record == NULL) {
        perror("malloc");
        return -1;
    }
    
    ls->size = BLOCK;
    ls->entries = 0;
    
    return 0;
}


int listing_clear(struct listing_t *ls)
{
    if(ls->record)
        free(ls->record);

    return 0;
}


int listing_blank(struct listing_t *ls)
{
    ls->entries = 0;
    return 0;
}


int listing_add(struct listing_t *ls, struct record_t *lr)
{
    struct record_t **ln;

    if(ls->entries == ls->size) {
        ln = realloc(ls->record, sizeof(struct listing_t) * ls->size * 2);
        
        if(!ln) {
            perror("realloc");
            return -1;
        }
        
        ls->record = ln;
        ls->size *= 2;
    }
    
    ls->record[ls->entries++] = lr;
    
    return 0;
}


int listing_sort(struct listing_t *ls)
{
    int i, changed;
    struct record_t *re;
    
    do {
        changed = 0;
        
        for(i = 0; i < ls->entries - 1; i++) {
            if(strcmp(ls->record[i]->name, ls->record[i + 1]->name) > 0) {
                re = ls->record[i];
                ls->record[i] = ls->record[i + 1];
                ls->record[i + 1] = re;
                changed++;
            }
        }
    } while(changed);
    
    return 0;
}


int listing_match(struct listing_t *src, struct listing_t *dest, char *match)
{
    int n;
    struct record_t *re;

    fprintf(stderr, "Matching '%s'\n", match);

    for(n = 0; n < src->entries; n++) {
        re = src->record[n];
        
        if(strmatch(match, re->name))
            listing_add(dest, re);
    }

    return 0;
}


int listing_debug(struct listing_t *ls)
{
    int n;

    for(n = 0; n < ls->entries; n++)
        fprintf(stderr, "%d: %s\n", n, ls->record[n]->name);
    
    return 0;
}
