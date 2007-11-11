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

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <SDL.h>
#include <SDL_ttf.h>

#include "interface.h"
#include "library.h"
#include "player.h"
#include "rig.h"
#include "timecoder.h"
#include "xwax.h"


/* Screen refresh time in milliseconds */

#define REFRESH 10


/* Font definitions */

#define FONT "Vera.ttf"
#define FONT_SIZE 10
#define FONT_SPACE 15

#define EM_FONT "VeraIt.ttf"

#define CLOCK_FONT FONT
#define CLOCK_FONT_SIZE 32

#define DECI_FONT FONT
#define DECI_FONT_SIZE 20

#define DETAIL_FONT "VeraMono.ttf"
#define DETAIL_FONT_SIZE 9


/* Screen dimensions */

#define BORDER 12
#define SPACER 8

#define CURSOR_WIDTH 4

#define PLAYER_HEIGHT 213
#define OVERVIEW_HEIGHT 16

#define LIBRARY_MIN_WIDTH 64
#define LIBRARY_MIN_HEIGHT 64

#define DEFAULT_WIDTH 960
#define DEFAULT_HEIGHT 720
#define DEFAULT_METER_SCALE 8

#define MAX_METER_SCALE 11

#define SEARCH_HEIGHT (FONT_SPACE)
#define STATUS_HEIGHT (FONT_SPACE)

#define RESULTS_ARTIST_WIDTH 200

#define CLOCKS_WIDTH 160

#define SPINNER_SIZE (CLOCK_FONT_SIZE * 2 - 6)
#define SCOPE_SIZE (CLOCK_FONT_SIZE * 2 - 6)
#define SCOPE_SCALE 2

#define SCROLLBAR_SIZE 10


/* Function key (F1-F12) definitions */

#define FUNC_LOAD 0
#define FUNC_RECUE 1
#define FUNC_DISCONNECT 2
#define FUNC_RECONNECT 3


/* State variables used to trigger certain actions */

#define UPDATE_NONE 0
#define UPDATE_REDRAW 1

#define RESULTS_REFINE 2
#define RESULTS_EXPAND 3


/* Macro functions */

#define MIN(x,y) ((x)<(y)?(x):(y))
#define SQ(x) ((x)*(x))

#define LOCK(sf) if(SDL_MUSTLOCK(sf)) SDL_LockSurface(sf)
#define UNLOCK(sf) if(SDL_MUSTLOCK(sf)) SDL_UnlockSurface(sf)
#define UPDATE(sf, rect) SDL_UpdateRect(sf, (rect)->x, (rect)->y, \
                                        (rect)->w, (rect)->h)


/* List of directories to use as search path for fonts. */

char *font_dirs[] = {
    "/usr/X11R6/lib/X11/fonts/TTF",
    "/usr/share/fonts/truetype/ttf-bitstream-vera",
    "/usr/share/fonts/TTF",
    NULL
};


TTF_Font *clock_font, *deci_font, *detail_font, *font, *em_font;

SDL_Color background_col = {0, 0, 0, 255},
    text_col = {224, 224, 224, 255},
    warn_col = {192, 64, 0, 255},
    ok_col = {32, 128, 3, 255},
    elapsed_col = {0, 32, 255, 255},
    remain_col = {0, 0, 64, 255},
    cursor_col = {192, 0, 0, 255},
    selected_col = {0, 48, 64, 255},
    detail_col = {128, 128, 128, 255},
    needle_col = {255, 255, 255, 255},
    vinyl_col = {0, 0, 0, 255},
    vinyl_ppm_col = {255, 255, 255, 255};

int spinner_angle[SPINNER_SIZE * SPINNER_SIZE];


struct rect_t {
    signed short x, y, w, h;
};


/* Split the given rectangle split v pixels from the top, with a gap
 * of 'space' between the output rectangles */

static void split_top(const struct rect_t *source, struct rect_t *upper,
                      struct rect_t *lower, int v, int space)
{
    int u;

    u = v + space;

    if(upper) {
        upper->x = source->x;
        upper->y = source->y;
        upper->w = source->w;
        upper->h = v;
    }

    if(lower) {
        lower->x = source->x;
        lower->y = source->y + u;
        lower->w = source->w;
        lower->h = source->h - u;
    }
}


/* As above, x pixels from the bottom */

static void split_bottom(const struct rect_t *source, struct rect_t *upper,
                         struct rect_t *lower, int v, int space)
{
    split_top(source, upper, lower, source->h - v - space, space);
}


/* As above, v pixels from the left */

static void split_left(const struct rect_t *source, struct rect_t *left,
                       struct rect_t *right, int v, int space)
{
    int u;

    u = v + space;

    if(left) {
        left->x = source->x;
        left->y = source->y;
        left->w = v;
        left->h = source->h;
    }

    if(right) {
        right->x = source->x + u;
        right->y = source->y;
        right->w = source->w - u;
        right->h = source->h;
    }
}


/* As above, v pixels from the right */

static void split_right(const struct rect_t *source, struct rect_t *left,
                        struct rect_t *right, int v, int space)
{
    split_left(source, left, right, source->w - v - space, space);
}


static void time_to_clock(char *buf, char *deci, int t)
{
    int minutes, seconds, frac, neg;

    if(t < 0) {
        t = abs(t);
        neg = 1;
    } else
        neg = 0;

    minutes = (t / 60 / 1000) % (60*60);
    seconds = (t / 1000) % 60;
    frac = t % 1000;
    
    if(neg)
        *buf++ = '-';

    sprintf(buf, "%02d:%02d.", minutes, seconds);
    sprintf(deci, "%03d", frac);
}


static int calculate_spinner_lookup(int *angle, int *distance, int size)
{
    int r, c, nr, nc;
    float theta, rat;

    for(r = 0; r < size; r++) {
        nr = r - size / 2;

        for(c = 0; c < size; c++) {
            nc = c - size / 2;

            if(nr == 0)
                theta = M_PI_2;

            else if(nc == 0) {
                theta = 0;
                
                if(nr < 0)
                    theta = M_PI;
            
            } else {
                rat = (float)(nc) / -nr;
                theta = atanf(rat);
                
                if(rat < 0)
                    theta += M_PI;
            }
            
            if(nc <= 0)
                theta += M_PI;
            
            angle[r * size + c]
                = ((int)(theta * 1024 / (M_PI * 2)) + 1024) % 1024;

            if(distance)
                distance[r * size + c] = sqrt(SQ(nc) + SQ(nr));
        }
    }    

    return 0;
}


/* Open a font, given the leafname. This scans the available font
 * directories for the file, to account for different software
 * distributions. */

static TTF_Font* open_font(const char *name, int size) {
    int r;
    char buf[256], **dir;
    struct stat st;
    TTF_Font *font;

    dir = &font_dirs[0];

    while(*dir) {
        
        sprintf(buf, "%s/%s", *dir, name);
        
        r = stat(buf, &st);

        if(r != -1) { /* something exists at this path */
            fprintf(stderr, "Loading font '%s', %dpt...\n", buf, size);
            
            font = TTF_OpenFont(buf, size);
            if(!font) {
                fputs("Font error: ", stderr);
                fputs(TTF_GetError(), stderr);
                fputc('\n', stderr);
            }            
            return font; /* or NULL */
        }
        
        if(errno != ENOENT) {
            perror("stat");
            return NULL;
        }
        
        dir++;
        continue;
    }

    fprintf(stderr, "Font '%s' cannot be found in", name);
    
    dir = &font_dirs[0];
    while(*dir) {
        fputc(' ', stderr);
        fputs(*dir, stderr);
        dir++;
    }
    fputc('.', stderr);
    fputc('\n', stderr);

    return NULL;
}


static int load_fonts(void)
{
    clock_font = open_font(CLOCK_FONT, CLOCK_FONT_SIZE);
    if(!clock_font)
        return -1;
    
    deci_font = open_font(DECI_FONT, DECI_FONT_SIZE);
    if(!deci_font)
        return -1;
    
    font = open_font(FONT, FONT_SIZE);
    if(!font)
        return -1;
    
    em_font = open_font(EM_FONT, FONT_SIZE);
    if(!em_font)
        return -1;
    
    detail_font = open_font(DETAIL_FONT, DETAIL_FONT_SIZE);
    if(!detail_font)
        return -1;
    
    return 0;
}


static void clear_fonts(void)
{
    TTF_CloseFont(font);
    TTF_CloseFont(em_font);
    TTF_CloseFont(clock_font);
    TTF_CloseFont(detail_font);
}


static Uint32 palette(SDL_Surface *sf, SDL_Color *col)
{
    return SDL_MapRGB(sf->format, col->r, col->g, col->b);
}


static int draw_font(SDL_Surface *sf, int x, int y, int w, int h,
                     const char *buf, TTF_Font *font,
                     SDL_Color fg, SDL_Color bg)
{
    SDL_Surface *rendered;
    SDL_Rect dst, src, fill;

    if(buf == NULL) {
        src.w = 0;
        src.h = 0;

    } else if(buf[0] == '\0') { /* SDL_ttf fails for empty string */
        src.w = 0;
        src.h = 0;

    } else {
        rendered = TTF_RenderText_Shaded(font, buf, fg, bg);
        
        src.x = 0;
        src.y = 0;
        src.w = MIN(w, rendered->w);
        src.h = MIN(h, rendered->h);
        
        dst.x = x;
        dst.y = y;
        
        SDL_BlitSurface(rendered, &src, sf, &dst);
        SDL_FreeSurface(rendered);
    } 

    /* Complete the remaining space with a blank rectangle */

    if(src.w < w) {
        fill.x = x + src.w;
        fill.y = y;
        fill.w = w - src.w;
        fill.h = h;   
        SDL_FillRect(sf, &fill, palette(sf, &bg));
    }

    if(src.h < h) {
        fill.x = x;
        fill.y = y + src.h;
        fill.w = src.w; /* the x-fill rectangle does the corner */
        fill.h = h - src.h;
        SDL_FillRect(sf, &fill, palette(sf, &bg));
    }

    return src.w;
}


static int draw_font_rect(SDL_Surface *surface, const struct rect_t *rect,
                          const char *buf, TTF_Font *font,
                          SDL_Color fg, SDL_Color bg)
{
    return draw_font(surface, rect->x, rect->y, rect->w, rect->h,
                     buf, font, fg, bg);
}


/* Draw the display of artist name and track name */

static void draw_track_summary(SDL_Surface *surface, const struct rect_t *rect,
                               struct track_t *track)
{
    const char *s;
    struct rect_t top, bottom;

    split_top(rect, &top, &bottom, FONT_SPACE, 0);

    if(track->artist)
        s = track->artist;
    else
        s = track->name;

    draw_font_rect(surface, &top, s, font, text_col, background_col);

    if(track->title)
        s = track->title;
    else
        s = track->name;

    draw_font_rect(surface, &bottom, s, em_font, text_col, background_col);
}


/* Draw a single clock, in hours:minutes.seconds format */

static void draw_clock(SDL_Surface *surface, const struct rect_t *rect, int t,
                       SDL_Color col)
{
    char hms[8], deci[8];
    short int v;
    int offset;
    struct rect_t sr;

    time_to_clock(hms, deci, t);
    
    v = draw_font_rect(surface, rect, hms, clock_font, col, background_col);
    
    offset = CLOCK_FONT_SIZE - DECI_FONT_SIZE * 1.04;

    sr.x = rect->x + v; 
    sr.y = rect->y + offset;
    sr.w = rect->w - v;
    sr.h = rect->h - offset;
    
    draw_font_rect(surface, &sr, deci, deci_font, col, background_col);
}


/* Draw the visual display of the input audio to the timecoder (the
 * 'scope') */

static void draw_scope(SDL_Surface *surface, const struct rect_t *rect,
                       struct timecoder_t *tc)
{
    int r, c, v, mid;
    Uint8 *p;

    mid = tc->mon_size / 2;

    for(r = 0; r < tc->mon_size; r++) {
        for(c = 0; c < tc->mon_size; c++) {
            p = surface->pixels
                + (rect->y + r) * surface->pitch
                + (rect->x + c) * surface->format->BytesPerPixel;

            v = tc->mon[r * tc->mon_size + c];

            if((r == mid || c == mid) && v < 64)
                v = 64;

            p[0] = v;
            p[1] = p[0];
            p[2] = p[1];            
        }
    }    
}


/* Draw the spinner which shows the rotational position of the record,
 * and matches the physical rotation of the vinyl record */

static void draw_spinner(SDL_Surface *surface, const struct rect_t *rect,
                         struct player_t *pl)
{
    int x, y, r, c, rangle, pangle, position;
    Uint8 *rp, *p;
    SDL_Color col;

    x = rect->x;
    y = rect->y;

    position = pl->position - pl->offset;
    rangle = (int)(pl->position * 1024 * 10 / 18 / TRACK_RATE) % 1024; 

    for(r = 0; r < SPINNER_SIZE; r++) {
        
        /* Store a pointer to this row of the framebuffer */
        
        rp = surface->pixels + (y + r) * surface->pitch;
        
        for(c = 0; c < SPINNER_SIZE; c++) {
            
            /* Use the lookup table to provide the angle at each
             * pixel */
            
            pangle = spinner_angle[r * SPINNER_SIZE + c];
            
            if(position < 0 || position >= pl->track->length)
                col = warn_col;
            else
                col = ok_col;
            
            if((rangle - pangle + 1024) % 1024 < 512) {
                col.r >>= 2;
                col.g >>= 2;
                col.b >>= 2;
            }
            
            /* Calculate the final pixel location and set it */
            
            p = rp + (x + c) * surface->format->BytesPerPixel;
            
            p[0] = col.b;
            p[1] = col.g;
            p[2] = col.r;  
        }
    }    
}


/* Draw the clocks which show time elapsed and time remaining */

static void draw_deck_clocks(SDL_Surface *surface, const struct rect_t *rect,
                             struct player_t *pl)
{
    int elapse, remain, pos;
    struct rect_t upper, lower;
    SDL_Color col;

    split_top(rect, &upper, &lower, CLOCK_FONT_SIZE, 0);

    pos = pl->position - pl->offset;

    elapse = (long long)pos * 1000 / TRACK_RATE;
    remain = ((long long)pos - pl->track->length) * 1000 / TRACK_RATE;

    if(elapse < 0)
        col = warn_col;
    else if(remain <= 0)
        col = ok_col;
    else
        col = text_col;

    draw_clock(surface, &upper, elapse, col);

    if(remain > 0)
        col = warn_col;
    else
        col = text_col;

    if(pl->track->status == TRACK_STATUS_IMPORTING) {
        col.r >>= 2;
        col.g >>= 2;
        col.b >>= 2;
    }

    draw_clock(surface, &lower, remain, col);
}


/* Draw the high-level overview meter which shows the whole length
 * of the track */

static void draw_overview(SDL_Surface *surface, const struct rect_t *rect,
                          struct track_t *tr, int position)
{
    int x, y, w, h, r, c, sp, fade, bytes_per_pixel, pitch, height;
    Uint8 *pixels, *p;
    SDL_Color col;

    x = rect->x;
    y = rect->y;
    w = rect->w;
    h = rect->h;

    pixels = surface->pixels;
    bytes_per_pixel = surface->format->BytesPerPixel;
    pitch = surface->pitch;

    for(c = 0; c < w; c++) {

        /* Collect the correct meter value for this column */

        sp = (long long)tr->length * c / w;

        if(sp < tr->length) /* account for rounding */
            height = track_get_overview(tr, sp) * h / 256;
        else
            height = 0;

        /* Choose a base colour to display in */

        if(!tr->length)
            col = background_col;
        else if(position > tr->length - TRACK_RATE * 20)
            col = warn_col;
        else
            col = elapsed_col;

        fade = 0;

        if(tr->status == TRACK_STATUS_IMPORTING)
            fade++;

        if(tr->length && c <= (long long)position * w / tr->length)
            fade++;

        col.r >>= fade;
        col.g >>= fade;
        col.b >>= fade;

        /* Store a pointer to this column of the framebuffer */

        p = pixels + y * pitch + (x + c) * bytes_per_pixel;

        r = h;
        while(r > height) {
            p[0] = col.b >> 2;
            p[1] = col.g >> 2;
            p[2] = col.r >> 2;
            p += pitch;
            r--;
        }
        while(r) {
            p[0] = col.b;
            p[1] = col.g;
            p[2] = col.r;
            p += pitch;
            r--;
        }
    }
}


/* Draw the close-up meter, which can be zoomed to a level set by
 * 'scale' */

static void draw_closeup(SDL_Surface *surface, const struct rect_t *rect,
                         struct track_t *tr, int position, int scale)
{
    int x, y, w, h, r, c, sp, fade, bytes_per_pixel, pitch, height;
    Uint8 *pixels, *p;
    SDL_Color col;

    x = rect->x;
    y = rect->y;
    w = rect->w;
    h = rect->h;

    pixels = surface->pixels;
    bytes_per_pixel = surface->format->BytesPerPixel;
    pitch = surface->pitch;

    for(c = 0; c < w; c++) {

        /* Work out the meter height in pixels for this column */

        sp = position - (position % (1 << scale))
            + ((c - w / 2) << scale);

        if(sp < tr->length && sp > 0)
            height = track_get_ppm(tr, sp) * h / 256;
        else
            height = 0;

        /* Select the appropriate colour */

        if(c == w / 2) {
            col = needle_col;
            fade = 1;
        } else {
            col = elapsed_col;
            fade = 3;
        }

        /* Get a pointer to the top of the column, and increment
         * it for each row */

        p = pixels + y * pitch + (x + c) * bytes_per_pixel;

        r = h;
        while(r > height) {
            p[0] = col.b >> fade;
            p[1] = col.g >> fade;
            p[2] = col.r >> fade;
            p += pitch;
            r--;
        }
        while(r) {
            p[0] = col.b;
            p[1] = col.g;
            p[2] = col.r;
            p += pitch;
            r--;
        }
    }
}


/* Draw the audio meters for a deck */                         

static void draw_meters(SDL_Surface *surface, const struct rect_t *rect,
                        struct track_t *tr, int position, int scale)
{
    struct rect_t overview, closeup;

    split_top(rect, &overview, &closeup, OVERVIEW_HEIGHT, SPACER);

    if(closeup.h > OVERVIEW_HEIGHT)
        draw_overview(surface, &overview, tr, position);
    else
        closeup = *rect;

    draw_closeup(surface, &closeup, tr, position, scale);
}


/* Draw the current playback status -- clocks, spinner and scope */

static void draw_deck_top(SDL_Surface *surface, const struct rect_t *rect,
                          struct player_t *pl)
{
    struct rect_t clocks, left, right, spinner, scope;
    
    split_left(rect, &clocks, &right, CLOCKS_WIDTH, SPACER);

    /* If there is no timecoder to display information on, or not enough 
     * available space, just draw clocks which span the overall space */

    if(!pl->timecoder || right.w < 0) {
        draw_deck_clocks(surface, rect, pl);
        return;
    }

    draw_deck_clocks(surface, &clocks, pl);    

    split_right(&right, &left, &spinner, SPINNER_SIZE, SPACER);
    if(left.w < 0)
        return;
    split_bottom(&spinner, NULL, &spinner, SPINNER_SIZE, 0);
    draw_spinner(surface, &spinner, pl);

    split_right(&left, &clocks, &scope, SCOPE_SIZE, SPACER);
    if(clocks.w < 0)
        return;
    split_bottom(&scope, NULL, &scope, SCOPE_SIZE, 0);
    draw_scope(surface, &scope, pl->timecoder);
}


/* Draw the textual description of playback status, which includes
 * information on the timecode */

static void draw_deck_status(SDL_Surface *surface,
                                 const struct rect_t *rect,
                                 struct player_t *pl)
{
    char buf[128];
    int tc;
    
    if(pl->timecoder && (tc=timecoder_get_position(pl->timecoder, NULL)) != -1)
        sprintf(buf, "timecode:%d      ", tc);
    else
        sprintf(buf, "timecode:        ");
    
    sprintf(buf + 17, "pitch:%+0.2f (sync %0.2f %+4.0f = %+0.2f)  "
            "status:%s%s%s",
            pl->pitch,
            pl->sync_pitch,
            pl->last_difference,
            pl->pitch * pl->sync_pitch,
            pl->lost ? "LOST  " : "",
            pl->playing ? "PLAY  " : "",
            pl->reconnect ? "RECN  " : "");
    
    draw_font_rect(surface, rect, buf, detail_font,
                   detail_col, background_col);
}


/* Draw a single deck */

static void draw_deck(SDL_Surface *surface, const struct rect_t *rect,
                      struct player_t *pl, int meter_scale)
{
    int position;
    struct rect_t track, top, meters, status, rest, lower;

    position = pl->position - pl->offset;

    split_top(rect, &track, &rest, FONT_SPACE * 2, 0);
    if(rest.h < 160)
        rest = *rect;
    else
        draw_track_summary(surface, &track, pl->track);

    split_top(&rest, &top, &lower, CLOCK_FONT_SIZE * 2, SPACER);
    if(lower.h < 64)
        lower = rest;
    else
        draw_deck_top(surface, &top, pl);
    
    split_bottom(&lower, &meters, &status, FONT_SPACE, SPACER); 
    if(meters.h < 64)
        meters = lower;
    else
        draw_deck_status(surface, &status, pl);

    draw_meters(surface, &meters, pl->track, position, meter_scale);
}


/* Draw all the decks in the system */

static void draw_decks(SDL_Surface *surface, const struct rect_t *rect,
                       int ndecks, struct player_t **player,
                       int meter_scale)
{
    int d, deck_width;
    struct rect_t single;

    deck_width = (rect->w - BORDER * (ndecks - 1)) / ndecks;

    single = *rect;
    single.w = deck_width;

    for(d = 0; d < ndecks; d++) {
        single.x = rect->x + (deck_width + BORDER) * d;
        draw_deck(surface, &single, player[d], meter_scale);
    }
}


/* Draw the status bar */

static void draw_status(SDL_Surface *sf, const struct rect_t *rect,
                        const char *text)
{
    draw_font(sf, rect->x, rect->y, rect->w, FONT_SPACE, text, detail_font,
              detail_col, background_col);
}


/* Draw the search field which the user types into */

static void draw_search(SDL_Surface *surface, const struct rect_t *rect,
                        const char *search, int entries)
{
    const char *buf;
    char cm[32];
    int x, y, w, s;
    SDL_Rect cursor;
    
    x = rect->x;
    y = rect->y;
    w = rect->w;

    if(search[0] != '\0')
        buf = search;
    else
        buf = NULL;
    
    x += SCROLLBAR_SIZE + SPACER;
    w -= SCROLLBAR_SIZE + SPACER;

    s = draw_font(surface, x, y, w, FONT_SPACE, buf, font,
                  text_col, background_col);

    x += s;
    w -= s;

    cursor.x = x;
    cursor.y = y;
    cursor.w = CURSOR_WIDTH;
    cursor.h = FONT_SPACE;

    SDL_FillRect(surface, &cursor, palette(surface, &cursor_col));
    
    if(entries > 1)
        sprintf(cm, "%d matches", entries);
    else if(entries > 0)
        sprintf(cm, "1 match");
    else
        sprintf(cm, "no matches");

    x += CURSOR_WIDTH + SPACER;
    w -= CURSOR_WIDTH + SPACER;
    
    w = draw_font(surface, x, y, w, FONT_SPACE, cm, em_font,
                  detail_col, background_col);
}


/* Display a record library listing, with scrollbar and current
 * selection. Return the number of lines which fit on the display. */

static int draw_listing(SDL_Surface *surface, const struct rect_t *rect,
                        const struct listing_t *ls, int selected,
                        int view_offset)
{
    int x, y, w, h, n, r, ox;
    struct record_t *re;
    SDL_Rect box;
    SDL_Color col;
    
    x = rect->x;
    y = rect->y;
    w = rect->w;
    h = rect->h;

    ox = x;

    x += SCROLLBAR_SIZE + SPACER;
    w -= SCROLLBAR_SIZE + SPACER;

    for(n = 0; n + view_offset < ls->entries; n++) {
        re = ls->record[n + view_offset];
    
        if((n + 1) * FONT_SPACE > h) 
            break;

        r = y + n * FONT_SPACE;
    
        if(n + view_offset == selected)
            col = selected_col;
        else
            col = background_col;

        if(re->artist[0] == '\0') {
            draw_font(surface, x, r, w, FONT_SPACE, re->name, font,
                      text_col, col);
            
        } else {
            draw_font(surface, x, r, RESULTS_ARTIST_WIDTH, FONT_SPACE,
                      re->artist, font, text_col, col);
            
            box.x = x + RESULTS_ARTIST_WIDTH;
            box.y = r;
            box.w = SPACER;
            box.h = FONT_SPACE;
            
            SDL_FillRect(surface, &box, palette(surface, &col));
            
            draw_font(surface, x + RESULTS_ARTIST_WIDTH + SPACER, r,
                      w - RESULTS_ARTIST_WIDTH - SPACER, FONT_SPACE,
                      re->title, em_font, text_col, col);
        }
    }

    /* Blank any remaining space */
    
    box.x = x;
    box.y = y + n * FONT_SPACE;
    box.w = w;
    box.h = h - (n * FONT_SPACE);
    
    SDL_FillRect(surface, &box, palette(surface, &background_col));
    
    /* Return to the origin and output the scrollbar -- now that we have
     * a value for n */

    x = ox;

    col = selected_col;
    col.r >>= 1;
    col.g >>= 1;
    col.b >>= 1;

    box.x = x;
    box.y = y;
    box.w = SCROLLBAR_SIZE;
    box.h = h;

    SDL_FillRect(surface, &box, palette(surface, &col));

    if(ls->entries > 0) {
        box.x = x;
        box.y = y + h * view_offset / ls->entries;
        box.w = SCROLLBAR_SIZE;
        box.h = h * n / ls->entries;
        
        SDL_FillRect(surface, &box, palette(surface, &selected_col));
    }

    return n;
}


/* Display the music library, which consists of the query, and search
 * results. Return the number of lines in the library display. */

static int draw_library(SDL_Surface *surface, const struct rect_t *rect,
                        const char *search, const struct listing_t *listing,
                        int selected, int view_offset)
{
    int lines;
    struct rect_t rsearch, rresults;

    split_top(rect, &rsearch, &rresults, SEARCH_HEIGHT, SPACER);
    draw_search(surface, &rsearch, search, listing->entries);
    lines = draw_listing(surface, &rresults, listing, selected, view_offset);

    return lines;
}


/* Initiate the process of loading a record from the library into a
 * track in memory */

static int do_loading(struct track_t *track, struct record_t *record)
{
    fprintf(stderr, "Loading '%s'.\n", record->name);

    track_abort(track);
    track_wait(track);

    track_import(track, record->pathname);

    if(strlen(record->artist))
        track->artist = record->artist;
    else
        track->artist = NULL;

    if(strlen(record->title))
        track->title = record->title;
    else
        track->title = NULL;
    
    track->name = record->name;
    
    return 0;
}


static SDL_Surface* set_size(int w, int h, struct rect_t *rect)
{
    SDL_Surface *surface;

    surface = SDL_SetVideoMode(w, h, 32, SDL_RESIZABLE);
    if(surface == NULL) {
        fprintf(stderr, "%s\n", SDL_GetError());
        return NULL;
    }
    
    rect->x = BORDER;
    rect->y = BORDER;
    rect->w = w - 2 * BORDER;
    rect->h = h - 2 * BORDER;

    fprintf(stderr, "New interface size is %dx%d.\n", w, h);
    
    return surface;
}


static Uint32 ticker(Uint32 interval, void *p)
{
    SDL_Event event;
    
    if(!SDL_PeepEvents(&event, 1, SDL_PEEKEVENT, SDL_EVENTMASK(SDL_USEREVENT)))
    {
        event.type = SDL_USEREVENT;
        SDL_PushEvent(&event);
    }        

    return interval;
}


void interface_init(struct interface_t *in)
{
    int n;

    for(n = 0; n < MAX_PLAYERS; n++)
        in->player[n] = NULL;

    in->listing = NULL;
}


int interface_run(struct interface_t *in)
{
    int selected, view_offset, meter_scale,
        library_lines, p, search_len,
        finished,
        library_update, decks_update, status_update,
        deck, func;
    char search[256];
    const char *status = BANNER;

    SDL_Event event;
    SDLKey key;
    SDLMod mod;
    SDL_TimerID timer;
    SDL_Surface *surface;

    struct rect_t rworkspace, rplayers, rlibrary, rstatus, rtmp;
    struct listing_t *results, *refine, *ltmp, la, lb;
    struct player_t *pl;

    finished = 0;
    
    listing_init(&la);
    listing_init(&lb);
    results = &la;
    refine = &lb;

    meter_scale = DEFAULT_METER_SCALE;
    
    search[0] = '\0';
    search_len = 0;
    library_lines = 0;
    selected = 0;
    view_offset = 0;

    for(p = 0; p < in->timecoders; p++)
        timecoder_monitor_init(in->timecoder[p], SCOPE_SIZE, SCOPE_SCALE);
    
    calculate_spinner_lookup(spinner_angle, NULL, SPINNER_SIZE);

    fprintf(stderr, "Initialising SDL...\n");
    
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == -1) { 
        fprintf(stderr, "%s\n", SDL_GetError());
        return -1;
    }
    SDL_WM_SetCaption(BANNER, NULL);
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

    /* Initialise the fonts */
    
    if(TTF_Init() == -1) {
        fprintf(stderr, "%s\n", TTF_GetError());
        return -1;
    }

    if(load_fonts() == -1)
        return -1;

    /* Initialise the screen surface */

    surface = set_size(DEFAULT_WIDTH, DEFAULT_HEIGHT, &rworkspace);
    if(!surface)
        return -1;

    decks_update = UPDATE_REDRAW;
    status_update = UPDATE_REDRAW;
    library_update = RESULTS_EXPAND;

    /* The final action is to add the timer which triggers refresh */

    timer = SDL_AddTimer(REFRESH, ticker, (void*)in);

    while(!finished && SDL_WaitEvent(&event) >= 0) {

        switch(event.type) {
            
        case SDL_QUIT:
            finished = 1;
            break;

        case SDL_VIDEORESIZE:
            surface = set_size(event.resize.w, event.resize.h, &rworkspace);
            if(!surface)
                return -1;

            if(library_update < UPDATE_REDRAW)
                library_update = UPDATE_REDRAW;

            if(decks_update < UPDATE_REDRAW)
                decks_update = UPDATE_REDRAW;

            if(status_update < UPDATE_REDRAW)
                status_update = UPDATE_REDRAW;

            break;
            
        case SDL_USEREVENT: /* request to update the clocks */

            if(decks_update < UPDATE_REDRAW)
                decks_update = UPDATE_REDRAW;
            
            break;

        case SDL_KEYDOWN:
            key = event.key.keysym.sym;
            mod = event.key.keysym.mod;
            
            if(key >= SDLK_a && key <= SDLK_z) {
                search[search_len] = (key - SDLK_a) + 'a';
                search[++search_len] = '\0';
                library_update = RESULTS_REFINE;

            } else if(key >= SDLK_0 && key <= SDLK_9) {
                search[search_len] = (key - SDLK_0) + '0';
                search[++search_len] = '\0';
                library_update = RESULTS_REFINE;
                
            } else if(key == SDLK_SPACE) {
                search[search_len] = ' ';
                search[++search_len] = '\0';
                library_update = RESULTS_REFINE;
                
            } else if(key == SDLK_BACKSPACE && search_len > 0) {
                search[--search_len] = '\0';
                library_update = RESULTS_EXPAND;

            } else if(key == SDLK_UP) {
                selected--;

                if(selected < view_offset)
                    view_offset -= library_lines / 2;

                library_update = UPDATE_REDRAW;
    
            } else if(key == SDLK_DOWN) {
                selected++;
                
                if(selected > view_offset + library_lines - 1)
                    view_offset += library_lines / 2;
                
                library_update = UPDATE_REDRAW;

            } else if(key == SDLK_PAGEUP) {
                view_offset -= library_lines;

                if(selected > view_offset + library_lines - 1)
                    selected = view_offset + library_lines - 1;

                library_update = UPDATE_REDRAW;

            } else if(key == SDLK_PAGEDOWN) {
                view_offset += library_lines;
                
                if(selected < view_offset)
                    selected = view_offset;

                library_update = UPDATE_REDRAW;

            } else if(key == SDLK_EQUALS) {
                meter_scale--;
 
                if(meter_scale < 0)
                    meter_scale = 0;
                
                fprintf(stderr, "Meter scale decreased to %d\n", meter_scale);
                
            } else if(key == SDLK_MINUS) {
                meter_scale++;
                
                if(meter_scale > MAX_METER_SCALE)
                    meter_scale = MAX_METER_SCALE;
                
                fprintf(stderr, "Meter scale increased to %d\n", meter_scale);
                
            } else if(key >= SDLK_F1 && key <= SDLK_F12) {

                /* Handle the function key press in groups of four --
                 * F1-F4 (deck 0), F5-F8 (deck 1) etc. */

                func = (key - SDLK_F1) % 4;
                deck = (key - SDLK_F1) / 4;
                
                if(deck < in->players) {
                    pl = in->player[deck];
                    
                    if(mod & KMOD_SHIFT) {
                        if(func < in->timecoders)
                            player_connect_timecoder(pl, in->timecoder[func]);
                        
                    } else switch(func) {
                        
                    case FUNC_LOAD:
                        if(selected != -1) 
                            do_loading(pl->track, results->record[selected]);
                        break;
                        
                    case FUNC_RECUE:
                        player_recue(pl);
                        break;
                        
                    case FUNC_DISCONNECT:
                        player_disconnect_timecoder(pl);
                        break;
                        
                    case FUNC_RECONNECT:
                        player_connect_timecoder(pl, in->timecoder[deck]);
                        break;
                    }
                }
            }

            break;

        } /* switch(event.type) */

        /* Split the display into the various areas. If an area is too
         * small, abandon any actions to happen in that area. */

        split_bottom(&rworkspace, &rtmp, &rstatus, STATUS_HEIGHT, SPACER);
        if(rtmp.h < 128 || rtmp.w < 0) {
            rtmp = rworkspace;
            status_update = UPDATE_NONE;
        }

        split_top(&rtmp, &rplayers, &rlibrary, PLAYER_HEIGHT, SPACER);        
        if(rlibrary.h < LIBRARY_MIN_HEIGHT || rlibrary.w < LIBRARY_MIN_WIDTH) {
            rplayers = rtmp;
            library_update = UPDATE_NONE;
        }

        if(rplayers.h < 0 || rplayers.w < 0)
            decks_update = UPDATE_NONE;

        /* Re-search the library for based on the new criteria */
        
        if(library_update == RESULTS_EXPAND) {
            listing_blank(results);
            listing_match(in->listing, results, search);
            
        } else if(library_update == RESULTS_REFINE) {
            listing_blank(refine);
            listing_match(results, refine, search);
            
            /* Swap the a and b results lists */
            
            ltmp = results;
            results = refine;
            refine = ltmp;
        }

        /* If there's been a change to the library search results,
         * check them over and display them. */

        if(library_update >= UPDATE_REDRAW) {
            if(selected < 0)
                selected = 0;
            
            if(selected >= results->entries)
                selected = results->entries - 1;

            /* After the above, if selected == -1, there is no
             * valid item selected; 0 means the first item */

            if(view_offset > results->entries - library_lines)
                view_offset = results->entries - library_lines;
            
            if(view_offset < 0)
                view_offset = 0;

            if(selected != -1)
                status = results->record[selected]->pathname;
            else
                status = "No search results found";
            status_update = UPDATE_REDRAW;
          
            LOCK(surface);
            library_lines = draw_library(surface, &rlibrary, search, results,
                                         selected, view_offset);
            UNLOCK(surface);
            UPDATE(surface, &rlibrary);
            library_update = UPDATE_NONE;
        }

        /* If there's been a change to status, redisplay it */

        if(status_update >= UPDATE_REDRAW) {
            LOCK(surface);
            draw_status(surface, &rstatus, status);
            UNLOCK(surface);
            UPDATE(surface, &rstatus);
            status_update = UPDATE_NONE;
        }

        /* If it's due, redraw the players. This is triggered by the
         * timer event. */

        if(decks_update >= UPDATE_REDRAW) {
            LOCK(surface);
            draw_decks(surface, &rplayers, in->players, in->player,
                       meter_scale);
            UNLOCK(surface);
            UPDATE(surface, &rplayers);
            decks_update = UPDATE_NONE;
        }

        /* Enter wait state for any tracks. This must happen in this
         * thread, as the child process is a child of this thread. */

        for(p = 0; p < in->players; p++) 
            track_wait(in->player[p]->track);

    } /* main loop */

    SDL_RemoveTimer(timer);

    for(p = 0; p < in->timecoders; p++)
        timecoder_monitor_clear(in->timecoder[p]);
    
    listing_clear(&la);
    listing_clear(&lb);

    clear_fonts();

    TTF_Quit();
    SDL_Quit();
    
    return 0;
}
