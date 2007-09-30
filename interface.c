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

#define PLAYER_HEIGHT (FONT_SPACE + FONT_SPACE + CLOCK_FONT_SIZE \
                       + CLOCK_FONT_SIZE + SPACER + PROGRESS_HEIGHT \
                       + SPACER + METER_HEIGHT + SPACER + FONT_SPACE)

#define PROGRESS_HEIGHT 16
#define METER_HEIGHT 64

#define DEFAULT_WIDTH 960
#define DEFAULT_HEIGHT 720
#define DEFAULT_METER_SCALE 8

#define MAX_METER_SCALE 11

#define AREA_WIDTH(il) ((il)->width - BORDER * 2)
#define AREA_HEIGHT(il) ((il)->height - BORDER * 2)

#define PLAYER_WIDTH(in) ((AREA_WIDTH((in)->local)-BORDER*((in)->players-1)) \
                          / (in)->players)

#define SEARCH_POSITION (PLAYER_HEIGHT + BORDER)

#define STATUS_POSITION(il) (AREA_HEIGHT(il) - FONT_SPACE)
#define STATUS_HEIGHT (FONT_SPACE)

#define LISTING_POSITION (SEARCH_POSITION + FONT_SPACE + SPACER)
#define LISTING_HEIGHT(il) (AREA_HEIGHT(il) - LISTING_POSITION \
                            - STATUS_HEIGHT - SPACER)

#define RESULTS_ARTIST_WIDTH 200

#define CLOCKS_WIDTH 170

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

#define RESULTS_LEAVE 0
#define RESULTS_REDISPLAY 1
#define RESULTS_REFINE 2
#define RESULTS_EXPAND 3

#define PLAYERS_LEAVE 0
#define PLAYERS_REDISPLAY 1


/* Macro functions */

#define MIN(x,y) ((x)<(y)?(x):(y))
#define MAX(x,y) ((x)>(y)?(x):(y))

#define SQ(x) ((x)*(x))


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


struct interface_local_t {
    int width, height;
    short int meter_scale, result_lines;
    SDL_Surface *surface;
};


int spinner_angle[SPINNER_SIZE * SPINNER_SIZE];


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


static Uint32 palette(SDL_Surface *sf, SDL_Color *col)
{
    return SDL_MapRGB(sf->format, col->r, col->g, col->b);
}


static int draw_font(SDL_Surface *sf, int x, int y, int w, int h, char *buf,
                     TTF_Font *font, SDL_Color fg, SDL_Color bg)
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


static void display_timecoder(struct interface_local_t *il, int x, int y,
                              struct timecoder_t *tc)
{
    int r, c, v, mid;
    Uint8 *p;

    mid = tc->mon_size / 2;

    for(r = 0; r < tc->mon_size; r++) {
        for(c = 0; c < tc->mon_size; c++) {
            p = il->surface->pixels
                + (y + r) * il->surface->pitch
                + (x + c) * il->surface->format->BytesPerPixel;
            
            v = tc->mon[r * tc->mon_size + c];
            
            if((r == mid || c == mid) && v < 64)
                v = 64;
            
            p[0] = v;
            p[1] = p[0];
            p[2] = p[1];            
        }
    }
}


static void display_player(struct interface_local_t *il, int x, int y, int w,
                           struct player_t *pl)
{
    char hms[8], deci[8], buf[128], *s;
    int elapse, remain, rangle, pangle, pos, sp, tc;
    short int r, c, v, ox, oy, fade;
    SDL_Colour col;
    SDL_Rect rect;
    Uint8 *p;
    unsigned char m;

    ox = x;

    pos = pl->position - pl->offset;
    
    /* Track description -- artist and title, or just the name */

    if(pl->track->artist)
        s = pl->track->artist;
    else
        s = pl->track->name;
    
    draw_font(il->surface, x, y, w, FONT_SPACE, s, font, text_col,
              background_col);

    y += FONT_SPACE;
    
    if(pl->track->title)
        s = pl->track->title;
    else
        s = pl->track->name;
    
    draw_font(il->surface, x, y, w, FONT_SPACE, s, em_font, text_col,
              background_col);
    
    y += FONT_SPACE;    
    oy = y;
    
    x += w - SPINNER_SIZE - SPACER - SCOPE_SIZE;
    y += CLOCK_FONT_SIZE * 2 - MAX(SPINNER_SIZE, SCOPE_SIZE);
    
    /* Display the timecoder status before displaying the clocks */

    if(!pl->timecoder) {
        rect.x = x;
        rect.y = y;
        rect.w = SCOPE_SIZE + SPACER + SPINNER_SIZE;
        rect.h = MAX(SCOPE_SIZE, SPINNER_SIZE);
        
        SDL_FillRect(il->surface, &rect,
                     palette(il->surface, &background_col));
        
    } else {

        /* Timecode scope */
        
        if(pl->timecoder && pl->timecoder->mon)
            display_timecoder(il, x, y, pl->timecoder);
        
        x += SCOPE_SIZE + SPACER;
        
        /* Spinning record */
        
        rangle = (int)(pl->position * 1024 * 10 / 18 / TRACK_RATE) % 1024; 
        
        for(r = 0; r < SPINNER_SIZE; r++) {
            for(c = 0; c < SPINNER_SIZE; c++) {
                
                p = il->surface->pixels
                    + (y + r) * il->surface->pitch
                    + (x + c) * il->surface->format->BytesPerPixel;
                
                /* Use the lookup table to provide the angle at each
                 * pixel */

                pangle = spinner_angle[r * SPINNER_SIZE + c];
                
                if(pos < 0 || pos > pl->track->length)
                    col = warn_col;
                else
                    col = ok_col;
                
                if((rangle - pangle + 1024) % 1024 < 512) {
                    col.r >>= 2;
                    col.g >>= 2;
                    col.b >>= 2;
                }
                
                p[0] = col.b;
                p[1] = col.g;
                p[2] = col.r;  
            }
        }
    }
    
    x = ox;
    y = oy;
    
    /* Clocks */
 
    elapse = (long long)pos * 1000 / TRACK_RATE;
    remain = ((long long)pos - pl->track->length) * 1000 / TRACK_RATE;
    
    if(elapse < 0)
        col = warn_col;
    else if(remain <= 0)
        col = ok_col;
    else
        col = text_col;

    time_to_clock(hms, deci, elapse);

    v = draw_font(il->surface, x, y, CLOCKS_WIDTH, CLOCK_FONT_SIZE , hms,
                  clock_font, col, background_col);

    draw_font(il->surface, x + v, y + CLOCK_FONT_SIZE - DECI_FONT_SIZE * 1.04,
              CLOCKS_WIDTH - w, DECI_FONT_SIZE, deci, deci_font,
              col, background_col);

    y += CLOCK_FONT_SIZE;
    
    if(remain > 0)
        col = warn_col;
    else
        col = text_col;
    
    if(pl->track->status == TRACK_STATUS_IMPORTING) {
        col.r >>= 2;
        col.g >>= 2;
        col.b >>= 2;
    }

    time_to_clock(hms, deci, remain);

    v = draw_font(il->surface, x, y, CLOCKS_WIDTH, CLOCK_FONT_SIZE, hms,
                  clock_font, col, background_col);

    draw_font(il->surface, x + v, y + CLOCK_FONT_SIZE - DECI_FONT_SIZE * 1.04,
              CLOCKS_WIDTH - w, DECI_FONT_SIZE, deci, deci_font,
              col, background_col);
    
    y += CLOCK_FONT_SIZE + SPACER;
    
    /* Progress bar */

    for(c = 0; c < w; c++) {

        /* Collect the correct meter value for this column */
        
        sp = (long long)pl->track->length * c / w;
        
        if(sp < pl->track->length) /* account for rounding */
            m = track_get_overview(pl->track, sp);
        else
            m = 0;
        
        /* Work out a base colour to display in */
        
        if(!pl->track->length)
            col = background_col;
        else if(pos > pl->track->length - TRACK_RATE * 20)
            col = warn_col;
        else
            col = elapsed_col;
     
        if(pl->track->status == TRACK_STATUS_IMPORTING) {
            col.r >>= 1;
            col.g >>= 1;
            col.b >>= 1;
        }
        
        if(pl->track->length && c <= (long long)pos * w / pl->track->length) {
            col.r >>= 1;
            col.g >>= 1;
            col.b >>= 1;
        }
        
        for(r = 0; r < PROGRESS_HEIGHT; r++) {    
            
            p = il->surface->pixels
                + (y + r) * il->surface->pitch
                + (x + c) * il->surface->format->BytesPerPixel;
            
            /* Change the colour according to the audio meter */
            
            if((PROGRESS_HEIGHT - r) * 256 > (int)m * PROGRESS_HEIGHT) {
                p[0] = col.b >> 2;
                p[1] = col.g >> 2;
                p[2] = col.r >> 2;

            } else {
                p[0] = col.b;
                p[1] = col.g;
                p[2] = col.r;
            }
        }
    }
    
    y += PROGRESS_HEIGHT + SPACER;

    /* Close-up meter */

    for(c = 0; c < w; c++) {
        
        sp = pos - (pos % (1 << il->meter_scale))
            + ((c - w / 2) << il->meter_scale);
        
        if(sp < pl->track->length && sp > 0)
            m = track_get_ppm(pl->track, sp);
        else
            m = 0;

        if(c == w / 2) {
            col = needle_col;
            fade = 1;
        } else {
            col = elapsed_col;
            fade = 3;
        }

        for(r = 0; r < METER_HEIGHT; r++) {

            p = il->surface->pixels
                + (y + r) * il->surface->pitch
                + (x + c) * il->surface->format->BytesPerPixel;
           
            if((METER_HEIGHT - r) * 256 > (int)m * METER_HEIGHT) {
                p[0] = col.b >> fade;
                p[1] = col.g >> fade;
                p[2] = col.r >> fade;
            } else {
                p[0] = col.b;
                p[1] = col.g;
                p[2] = col.r;
            }
        }
    }

    y += METER_HEIGHT + SPACER;

    /* Status text */
    
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
    
    draw_font(il->surface, x, y, w, FONT_SPACE, buf, detail_font, detail_col,
              background_col);
}


static void display_status(struct interface_local_t *il, int x, int y, int w,
                           char *text)
{
    draw_font(il->surface, x, y, w, FONT_SPACE, text, detail_font,
              detail_col, background_col);
}


static void display_search(struct interface_local_t *il, int x, int y, int w,
                           char *search, int entries)
{
    SDL_Rect rect;
    char *buf, cm[32];
    int s;
    
    if(search[0] != '\0')
        buf = search;
    else
        buf = NULL;
    
    x += SCROLLBAR_SIZE + SPACER;
    w -= SCROLLBAR_SIZE + SPACER;

    s = draw_font(il->surface, x, y, w, FONT_SPACE, buf, font,
                  text_col, background_col);

    x += s;
    w -= s;

    rect.x = x;
    rect.y = y;
    rect.w = CURSOR_WIDTH;
    rect.h = FONT_SPACE;

    SDL_FillRect(il->surface, &rect, palette(il->surface, &cursor_col));
    
    if(entries > 1)
        sprintf(cm, "%d matches", entries);
    else if(entries > 0)
        sprintf(cm, "1 match");
    else
        sprintf(cm, "no matches");

    x += CURSOR_WIDTH + SPACER;
    w -= CURSOR_WIDTH + SPACER;
    
    w = draw_font(il->surface, x, y, w, FONT_SPACE, cm, em_font,
                  detail_col, background_col);
}


static void display_results(struct interface_local_t *il, int x, int y, 
                            int w, int h, struct listing_t *ls, int selected,
                            int view)
{
    SDL_Rect rect;
    SDL_Color col;
    int n, r, ox;
    struct record_t *re;
    
    ox = x;

    x += SCROLLBAR_SIZE + SPACER;
    w -= SCROLLBAR_SIZE + SPACER;

    for(n = 0; n + view < ls->entries; n++) {
        re = ls->record[n + view];
    
        if((n + 1) * FONT_SPACE > h) 
            break;

        r = y + n * FONT_SPACE;
    
        if(n + view == selected)
            col = selected_col;
        else
            col = background_col;

        if(re->artist[0] == '\0') {
            draw_font(il->surface, x, r, w, FONT_SPACE, re->name, font,
                      text_col, col);
            
        } else {
            draw_font(il->surface, x, r, RESULTS_ARTIST_WIDTH, FONT_SPACE,
                      re->artist, font, text_col, col);
            
            rect.x = x + RESULTS_ARTIST_WIDTH;
            rect.y = r;
            rect.w = SPACER;
            rect.h = FONT_SPACE;
            
            SDL_FillRect(il->surface, &rect, palette(il->surface, &col));
            
            draw_font(il->surface, x + RESULTS_ARTIST_WIDTH + SPACER, r,
                      w - RESULTS_ARTIST_WIDTH - SPACER, FONT_SPACE,
                      re->title, em_font, text_col, col);
        }
    }

    /* Blank any remaining space */
    
    rect.x = x;
    rect.y = y + n * FONT_SPACE;
    rect.w = w;
    rect.h = h - (n * FONT_SPACE);
    
    SDL_FillRect(il->surface, &rect, palette(il->surface, &background_col));
    
    /* Return to the origin and output the scrollbar -- now that we have
     * a value for n */

    x = ox;

    col = selected_col;
    col.r >>= 1;
    col.g >>= 1;
    col.b >>= 1;

    rect.x = x;
    rect.y = y;
    rect.w = SCROLLBAR_SIZE;
    rect.h = h;

    SDL_FillRect(il->surface, &rect, palette(il->surface, &col));

    if(ls->entries > 0) {
        rect.x = x;
        rect.y = y + h * view / ls->entries;
        rect.w = SCROLLBAR_SIZE;
        rect.h = h * n / ls->entries;
        
        SDL_FillRect(il->surface, &rect, palette(il->surface, &selected_col));
    }
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


static int set_size(struct interface_t *in, int w, int h)
{
    in->local->surface = SDL_SetVideoMode(w, h, 32, SDL_RESIZABLE);
    if(in->local->surface == NULL) {
        fprintf(stderr, "%s\n", SDL_GetError());
        return -1;
    }

    in->local->width = w;
    in->local->height = h;
    in->local->result_lines = LISTING_HEIGHT(in->local) / FONT_SPACE;

    fprintf(stderr, "New interface size is %dx%d.\n", w, h);
    
    return 0;
}


int interface_init(struct interface_t *in)
{
    int n;

    for(n = 0; n < MAX_PLAYERS; n++)
        in->player[n] = NULL;
    
    in->listing = NULL;
    in->local = NULL;

    return 0;
}


int interface_run(struct interface_t *in)
{
    SDL_Event event;
    SDLKey key;
    SDLMod mod;
    SDL_Rect rect;
    SDL_TimerID timer;
    int selected, view;
    short int p, c, search_len, finished, rstate, pstate, deck, func; 
    char search[256];
    
    struct interface_local_t local;
    struct listing_t *results, *refine, *ltmp, la, lb;
    struct player_t *pl;
    
    in->local = &local;
    
    listing_init(&la);
    listing_init(&lb);
    
    results = &la;
    refine = &lb;

    in->local->meter_scale = DEFAULT_METER_SCALE;

    finished = 0;
    
    search[0] = '\0';
    search_len = 0;
    
    selected = 0;
    view = 0;

    for(p = 0; p < in->timecoders; p++)
        timecoder_monitor_init(in->timecoder[p], SCOPE_SIZE, SCOPE_SCALE);
    
    calculate_spinner_lookup(spinner_angle, NULL, SPINNER_SIZE);

    fprintf(stderr, "Initialising SDL...\n");
    
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == -1) { 
        fprintf(stderr, "%s\n", SDL_GetError());
        return -1;
    }
    
    SDL_WM_SetCaption(BANNER, NULL);
    
    if(TTF_Init() == -1) {
        fprintf(stderr, "%s\n", TTF_GetError());
        return -1;
    }

    set_size(in, DEFAULT_WIDTH, DEFAULT_HEIGHT);
    
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

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
    
    timer = SDL_AddTimer(REFRESH, ticker, (void*)in);

    pstate = PLAYERS_REDISPLAY;
    rstate = RESULTS_EXPAND;

    while(!finished && SDL_WaitEvent(&event) >= 0) {

        switch(event.type) {
            
        case SDL_QUIT:
            finished = 1;
            break;

        case SDL_VIDEORESIZE:
            set_size(in, event.resize.w, event.resize.h);
            
            rect.x = 0;
            rect.y = 0;
            rect.w = event.resize.w;
            rect.h = event.resize.h;

            if(rstate < RESULTS_REDISPLAY)
                rstate = RESULTS_REDISPLAY;

            if(pstate < PLAYERS_REDISPLAY)
                pstate = PLAYERS_REDISPLAY;
            
            break;
            
        case SDL_USEREVENT: /* request to update the clocks */

            if(pstate < PLAYERS_REDISPLAY)
                pstate = PLAYERS_REDISPLAY;
            
            break;

        case SDL_KEYDOWN:
            key = event.key.keysym.sym;
            mod = event.key.keysym.mod;
            
            if(key >= SDLK_a && key <= SDLK_z) {
                search[search_len] = (key - SDLK_a) + 'a';
                search[++search_len] = '\0';
                rstate = RESULTS_REFINE;

            } else if(key >= SDLK_0 && key <= SDLK_9) {
                search[search_len] = (key - SDLK_0) + '0';
                search[++search_len] = '\0';
                rstate = RESULTS_REFINE;
                
            } else if(key == SDLK_SPACE) {
                search[search_len] = ' ';
                search[++search_len] = '\0';
                rstate = RESULTS_REFINE;
                
            } else if(key == SDLK_BACKSPACE && search_len > 0) {
                search[--search_len] = '\0';
                rstate = RESULTS_EXPAND;

            } else if(key == SDLK_UP) {
                selected--;

                if(selected < view)
                    view -= in->local->result_lines / 2;

                rstate = RESULTS_REDISPLAY;
    
            } else if(key == SDLK_DOWN) {
                selected++;
                
                if(selected > view + in->local->result_lines - 1)
                    view += in->local->result_lines / 2;
                
                rstate = RESULTS_REDISPLAY;

            } else if(key == SDLK_PAGEUP) {
                view -= in->local->result_lines;

                if(selected > view + in->local->result_lines - 1)
                    selected = view + in->local->result_lines - 1;

                rstate = RESULTS_REDISPLAY;

            } else if(key == SDLK_PAGEDOWN) {
                view += in->local->result_lines;
                
                if(selected < view)
                    selected = view;

                rstate = RESULTS_REDISPLAY;

            } else if(key == SDLK_EQUALS) {
                in->local->meter_scale--;
 
                if(in->local->meter_scale < 0)
                    in->local->meter_scale = 0;
                
                fprintf(stderr, "Meter scale decreased to %d\n",
                        in->local->meter_scale);
                
            } else if(key == SDLK_MINUS) {
                in->local->meter_scale++;
                
                if(in->local->meter_scale > MAX_METER_SCALE)
                    in->local->meter_scale = MAX_METER_SCALE;
                
                fprintf(stderr, "Meter scale increased to %d\n",
                        in->local->meter_scale);
                
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

        /* Re-search the library for based on the new criteria */
        
        if(rstate == RESULTS_EXPAND) {
            listing_blank(results);
            listing_match(in->listing, results, search);
            
        } else if(rstate == RESULTS_REFINE) {
            listing_blank(refine);
            listing_match(results, refine, search);
            
            /* Swap the a and b results lists */
            
            ltmp = results;
            results = refine;
            refine = ltmp;
        }
        
        /* If there's been a change to the library search results,
         * check them over and display them. */
        
        if(rstate >= RESULTS_REDISPLAY) {
            if(selected < 0)
                selected = 0;
            
            if(selected >= results->entries)
                selected = results->entries - 1;
            
            /* After the above, if selected == -1, there is no
             * valid item selected; 0 means the first item */
            
            if(view > results->entries - in->local->result_lines)
                view = results->entries - in->local->result_lines;
            
            if(view < 0)
                view = 0;
            
            if(SDL_MUSTLOCK(in->local->surface))
                SDL_LockSurface(in->local->surface);
            
            display_search(in->local, BORDER, BORDER + SEARCH_POSITION,
                           AREA_WIDTH(in->local), search,
                           results->entries);
            
            display_results(in->local, BORDER, BORDER + LISTING_POSITION,
                            AREA_WIDTH(in->local),
                            LISTING_HEIGHT(in->local),
                            results, selected, view);
            
            if(selected != -1) {
                display_status(in->local, BORDER,
                               BORDER + STATUS_POSITION(in->local),
                               AREA_WIDTH(in->local),
                               results->record[selected]->pathname);
            }                

            if(SDL_MUSTLOCK(in->local->surface))
                SDL_UnlockSurface(in->local->surface);
            
            SDL_UpdateRect(in->local->surface, BORDER,
                           BORDER + SEARCH_POSITION,
                           AREA_WIDTH(in->local),
                           AREA_HEIGHT(in->local) - SEARCH_POSITION);
        }

        /* If it's due, redraw the players. This is triggered by the
         * timer event. */
        
        if(pstate >= PLAYERS_REDISPLAY) {
            
            if(SDL_MUSTLOCK(in->local->surface))
                SDL_LockSurface(in->local->surface);
            
            for(p = 0; p < in->players; p++) {
                c = BORDER + (PLAYER_WIDTH(in) + BORDER) * p;
                display_player(in->local, c, BORDER, PLAYER_WIDTH(in),
                               in->player[p]);
            }

            if(SDL_MUSTLOCK(in->local->surface))
                SDL_UnlockSurface(in->local->surface);

            SDL_UpdateRect(in->local->surface, BORDER, BORDER,
                           AREA_WIDTH(in->local), PLAYER_HEIGHT);
        }

        /* Reset the state variables -- at the end of the loop so we can
         * force redraw on the first iteration */

        rstate = RESULTS_LEAVE;
        pstate = PLAYERS_LEAVE;

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

    TTF_CloseFont(font);
    TTF_CloseFont(em_font);
    TTF_CloseFont(clock_font);
    TTF_CloseFont(detail_font);

    TTF_Quit();
    SDL_Quit();
    
    return 0;
}
