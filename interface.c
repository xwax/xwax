/*
 * Copyright (C) 2011 Mark Hills <mark@pogo.org.uk>
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

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <SDL.h>
#include <SDL_ttf.h>

#include "interface.h"
#include "player.h"
#include "rig.h"
#include "selector.h"
#include "timecoder.h"
#include "xwax.h"

/* Screen refresh time in milliseconds */

#define REFRESH 10

/* Font definitions */

#define FONT "DejaVuSans.ttf"
#define FONT_SIZE 10
#define FONT_SPACE 15

#define EM_FONT "DejaVuSans-Oblique.ttf"

#define CLOCK_FONT FONT
#define CLOCK_FONT_SIZE 32

#define DECI_FONT FONT
#define DECI_FONT_SIZE 20

#define DETAIL_FONT "DejaVuSansMono.ttf"
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

#define SCROLLBAR_SIZE 10

#define METER_WARNING_TIME 20 /* time in seconds for "red waveform" warning */

/* Function key (F1-F12) definitions */

#define FUNC_LOAD 0
#define FUNC_RECUE 1
#define FUNC_TIMECODE 2

/* State variables used to trigger certain actions */

#define UPDATE_NONE 0
#define UPDATE_REDRAW 1

/* Types of SDL_USEREVENT */

#define EVENT_TICKER 0
#define EVENT_QUIT 1

/* Macro functions */

#define MIN(x,y) ((x)<(y)?(x):(y))
#define SQ(x) ((x)*(x))

#define LOCK(sf) if (SDL_MUSTLOCK(sf)) SDL_LockSurface(sf)
#define UNLOCK(sf) if (SDL_MUSTLOCK(sf)) SDL_UnlockSurface(sf)
#define UPDATE(sf, rect) SDL_UpdateRect(sf, (rect)->x, (rect)->y, \
                                        (rect)->w, (rect)->h)

/* List of directories to use as search path for fonts. */

static const char *font_dirs[] = {
    "/usr/X11R6/lib/X11/fonts/TTF",
    "/usr/share/fonts/truetype/ttf-dejavu/",
    "/usr/share/fonts/ttf-dejavu",
    "/usr/share/fonts/dejavu",
    "/usr/share/fonts/TTF",
    NULL
};

static TTF_Font *clock_font, *deci_font, *detail_font, *font, *em_font;

static SDL_Color background_col = {0, 0, 0, 255},
    text_col = {224, 224, 224, 255},
    warn_col = {192, 64, 0, 255},
    ok_col = {32, 128, 3, 255},
    elapsed_col = {0, 32, 255, 255},
    cursor_col = {192, 0, 0, 255},
    selected_col = {0, 48, 64, 255},
    detail_col = {128, 128, 128, 255},
    needle_col = {255, 255, 255, 255};

static int spinner_angle[SPINNER_SIZE * SPINNER_SIZE];

static pthread_t ph;
static struct deck *deck;
static size_t ndeck;
static struct selector selector;

struct rect {
    signed short x, y, w, h;
};

/*
 * Split the given rectangle split v pixels from the top, with a gap
 * of 'space' between the output rectangles
 *
 * Post: upper and lower are set
 */

static void split_top(const struct rect *source, struct rect *upper,
                      struct rect *lower, int v, int space)
{
    int u;

    u = v + space;

    if (upper) {
        upper->x = source->x;
        upper->y = source->y;
        upper->w = source->w;
        upper->h = v;
    }

    if (lower) {
        lower->x = source->x;
        lower->y = source->y + u;
        lower->w = source->w;
        lower->h = source->h - u;
    }
}

/*
 * As above, x pixels from the bottom
 */

static void split_bottom(const struct rect *source, struct rect *upper,
                         struct rect *lower, int v, int space)
{
    split_top(source, upper, lower, source->h - v - space, space);
}

/*
 * As above, v pixels from the left
 */

static void split_left(const struct rect *source, struct rect *left,
                       struct rect *right, int v, int space)
{
    int u;

    u = v + space;

    if (left) {
        left->x = source->x;
        left->y = source->y;
        left->w = v;
        left->h = source->h;
    }

    if (right) {
        right->x = source->x + u;
        right->y = source->y;
        right->w = source->w - u;
        right->h = source->h;
    }
}

/*
 * As above, v pixels from the right
 */

static void split_right(const struct rect *source, struct rect *left,
                        struct rect *right, int v, int space)
{
    split_left(source, left, right, source->w - v - space, space);
}

/*
 * Convert the given time (in milliseconds) to displayable time
 */

static void time_to_clock(char *buf, char *deci, int t)
{
    int minutes, seconds, frac;
    bool neg;

    if (t < 0) {
        t = abs(t);
        neg = true;
    } else
        neg = false;

    minutes = (t / 60 / 1000) % (60*60);
    seconds = (t / 1000) % 60;
    frac = t % 1000;

    if (neg)
        *buf++ = '-';

    sprintf(buf, "%02d:%02d.", minutes, seconds);
    sprintf(deci, "%03d", frac);
}

/*
 * Calculate a lookup which maps a position on screen to an angle,
 * relative to the centre of the spinner
 */

static void calculate_spinner_lookup(int *angle, int *distance, int size)
{
    int r, c, nr, nc;
    float theta, rat;

    for (r = 0; r < size; r++) {
        nr = r - size / 2;

        for (c = 0; c < size; c++) {
            nc = c - size / 2;

            if (nr == 0)
                theta = M_PI_2;

            else if (nc == 0) {
                theta = 0;

                if (nr < 0)
                    theta = M_PI;

            } else {
                rat = (float)(nc) / -nr;
                theta = atanf(rat);

                if (rat < 0)
                    theta += M_PI;
            }

            if (nc <= 0)
                theta += M_PI;

            /* The angles stored in the lookup table range from 0 to
             * 1023 (where 1024 is 360 degrees) */

            angle[r * size + c]
                = ((int)(theta * 1024 / (M_PI * 2)) + 1024) % 1024;

            if (distance)
                distance[r * size + c] = sqrt(SQ(nc) + SQ(nr));
        }
    }
}

/*
 * Open a font, given the leafname
 *
 * This scans the available font directories for the file, to account
 * for different software distributions.
 *
 * As this is an SDL (it is not an X11 app) we prefer to avoid the use
 * of fontconfig to select fonts.
 */

static TTF_Font* open_font(const char *name, int size) {
    int r;
    char buf[256];
    const char **dir;
    struct stat st;
    TTF_Font *font;

    dir = &font_dirs[0];

    while (*dir) {

        sprintf(buf, "%s/%s", *dir, name);

        r = stat(buf, &st);

        if (r != -1) { /* something exists at this path */
            fprintf(stderr, "Loading font '%s', %dpt...\n", buf, size);

            font = TTF_OpenFont(buf, size);
            if (!font) {
                fputs("Font error: ", stderr);
                fputs(TTF_GetError(), stderr);
                fputc('\n', stderr);
            }
            return font; /* or NULL */
        }

        if (errno != ENOENT) {
            perror("stat");
            return NULL;
        }

        dir++;
        continue;
    }

    fprintf(stderr, "Font '%s' cannot be found in", name);

    dir = &font_dirs[0];
    while (*dir) {
        fputc(' ', stderr);
        fputs(*dir, stderr);
        dir++;
    }
    fputc('.', stderr);
    fputc('\n', stderr);

    return NULL;
}

/*
 * Load all fonts
 */

static int load_fonts(void)
{
    clock_font = open_font(CLOCK_FONT, CLOCK_FONT_SIZE);
    if (!clock_font)
        return -1;

    deci_font = open_font(DECI_FONT, DECI_FONT_SIZE);
    if (!deci_font)
        return -1;

    font = open_font(FONT, FONT_SIZE);
    if (!font)
        return -1;

    em_font = open_font(EM_FONT, FONT_SIZE);
    if (!em_font)
        return -1;

    detail_font = open_font(DETAIL_FONT, DETAIL_FONT_SIZE);
    if (!detail_font)
        return -1;

    return 0;
}

/*
 * Free resources associated with fonts
 */

static void clear_fonts(void)
{
    TTF_CloseFont(clock_font);
    TTF_CloseFont(deci_font);
    TTF_CloseFont(font);
    TTF_CloseFont(em_font);
    TTF_CloseFont(detail_font);
}

static Uint32 palette(SDL_Surface *sf, SDL_Color *col)
{
    return SDL_MapRGB(sf->format, col->r, col->g, col->b);
}

/*
 * Draw text at the given coordinates
 *
 * Return: width of text drawn
 */

static int draw_text(SDL_Surface *sf, const struct rect *rect,
                     const char *buf, TTF_Font *font,
                     SDL_Color fg, SDL_Color bg)
{
    SDL_Surface *rendered;
    SDL_Rect dst, src, fill;

    if (buf == NULL) {
        src.w = 0;
        src.h = 0;

    } else if (buf[0] == '\0') { /* SDL_ttf fails for empty string */
        src.w = 0;
        src.h = 0;

    } else {
        rendered = TTF_RenderText_Shaded(font, buf, fg, bg);

        src.x = 0;
        src.y = 0;
        src.w = MIN(rect->w, rendered->w);
        src.h = MIN(rect->h, rendered->h);

        dst.x = rect->x;
        dst.y = rect->y;

        SDL_BlitSurface(rendered, &src, sf, &dst);
        SDL_FreeSurface(rendered);
    }

    /* Complete the remaining space with a blank rectangle */

    if (src.w < rect->w) {
        fill.x = rect->x + src.w;
        fill.y = rect->y;
        fill.w = rect->w - src.w;
        fill.h = rect->h;
        SDL_FillRect(sf, &fill, palette(sf, &bg));
    }

    if (src.h < rect->h) {
        fill.x = rect->x;
        fill.y = rect->y + src.h;
        fill.w = src.w; /* the x-fill rectangle does the corner */
        fill.h = rect->h - src.h;
        SDL_FillRect(sf, &fill, palette(sf, &bg));
    }

    return src.w;
}

/*
 * Draw a coloured rectangle
 */

static void draw_rect(SDL_Surface *surface, const struct rect *rect,
                      SDL_Color col)
{
    SDL_Rect b;

    b.x = rect->x;
    b.y = rect->y;
    b.w = rect->w;
    b.h = rect->h;
    SDL_FillRect(surface, &b, palette(surface, &col));
}

/*
 * Draw the record information in the deck
 */

static void draw_record(SDL_Surface *surface, const struct rect *rect,
                        const struct record *record)
{
    struct rect top, bottom;

    split_top(rect, &top, &bottom, FONT_SPACE, 0);

    draw_text(surface, &top, record->artist,
              font, text_col, background_col);
    draw_text(surface, &bottom, record->title,
              em_font, text_col, background_col);
}

/*
 * Draw a single time in milliseconds in hours:minutes.seconds format
 */

static void draw_clock(SDL_Surface *surface, const struct rect *rect, int t,
                       SDL_Color col)
{
    char hms[8], deci[8];
    short int v;
    int offset;
    struct rect sr;

    time_to_clock(hms, deci, t);

    v = draw_text(surface, rect, hms, clock_font, col, background_col);

    offset = CLOCK_FONT_SIZE - DECI_FONT_SIZE * 1.04;

    sr.x = rect->x + v;
    sr.y = rect->y + offset;
    sr.w = rect->w - v;
    sr.h = rect->h - offset;

    draw_text(surface, &sr, deci, deci_font, col, background_col);
}

/*
 * Draw the visual monitor of the input audio to the timecoder
 */

static void draw_scope(SDL_Surface *surface, const struct rect *rect,
                       struct timecoder *tc)
{
    int r, c, v, mid;
    Uint8 *p;

    mid = tc->mon_size / 2;

    for (r = 0; r < tc->mon_size; r++) {
        for (c = 0; c < tc->mon_size; c++) {
            p = surface->pixels
                + (rect->y + r) * surface->pitch
                + (rect->x + c) * surface->format->BytesPerPixel;

            v = tc->mon[r * tc->mon_size + c];

            if ((r == mid || c == mid) && v < 64)
                v = 64;

            p[0] = v;
            p[1] = p[0];
            p[2] = p[1];
        }
    }
}

/*
 * Draw the spinner
 *
 * The spinner shows the rotational position of the record, and
 * matches the physical rotation of the vinyl record.
 */

static void draw_spinner(SDL_Surface *surface, const struct rect *rect,
                         struct player *pl)
{
    int x, y, r, c, rangle, pangle;
    double elapsed, remain, rps;
    Uint8 *rp, *p;
    SDL_Color col;

    x = rect->x;
    y = rect->y;

    elapsed = player_get_elapsed(pl);
    remain = player_get_remain(pl);

    rps = timecoder_revs_per_sec(pl->timecoder);
    rangle = (int)(player_get_position(pl) * 1024 * rps) % 1024;

    if (elapsed < 0 || remain < 0)
        col = warn_col;
    else
        col = ok_col;

    for (r = 0; r < SPINNER_SIZE; r++) {

        /* Store a pointer to this row of the framebuffer */

        rp = surface->pixels + (y + r) * surface->pitch;

        for (c = 0; c < SPINNER_SIZE; c++) {

            /* Use the lookup table to provide the angle at each
             * pixel */

            pangle = spinner_angle[r * SPINNER_SIZE + c];

            /* Calculate the final pixel location and set it */

            p = rp + (x + c) * surface->format->BytesPerPixel;

            if ((rangle - pangle + 1024) % 1024 < 512) {
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
}

/*
 * Draw the clocks which show time elapsed and time remaining
 */

static void draw_deck_clocks(SDL_Surface *surface, const struct rect *rect,
                             struct player *pl, struct track *track)
{
    int elapse, remain;
    struct rect upper, lower;
    SDL_Color col;

    split_top(rect, &upper, &lower, CLOCK_FONT_SIZE, 0);

    elapse = player_get_elapsed(pl) * 1000;
    remain = player_get_remain(pl) * 1000;

    if (elapse < 0)
        col = warn_col;
    else if (remain > 0)
        col = ok_col;
    else
        col = text_col;

    draw_clock(surface, &upper, elapse, col);

    if (remain <= 0)
        col = warn_col;
    else
        col = text_col;

    if (track_is_importing(track)) {
        col.r >>= 2;
        col.g >>= 2;
        col.b >>= 2;
    }

    draw_clock(surface, &lower, -remain, col);
}

/*
 * Draw the high-level overview meter which shows the whole length
 * of the track
 */

static void draw_overview(SDL_Surface *surface, const struct rect *rect,
                          struct track *tr, int position)
{
    int x, y, w, h, r, c, sp, fade, bytes_per_pixel, pitch, height,
        current_position;
    Uint8 *pixels, *p;
    SDL_Color col;

    x = rect->x;
    y = rect->y;
    w = rect->w;
    h = rect->h;

    pixels = surface->pixels;
    bytes_per_pixel = surface->format->BytesPerPixel;
    pitch = surface->pitch;

    if (tr->length)
        current_position = (long long)position * w / tr->length;
    else
        current_position = 0;

    for (c = 0; c < w; c++) {

        /* Collect the correct meter value for this column */

        sp = (long long)tr->length * c / w;

        if (sp < tr->length) /* account for rounding */
            height = track_get_overview(tr, sp) * h / 256;
        else
            height = 0;

        /* Choose a base colour to display in */

        if (!tr->length) {
            col = background_col;
            fade = 0;
        } else if (c == current_position) {
            col = needle_col;
            fade = 1;
        } else if (position > tr->length - tr->rate * METER_WARNING_TIME) {
            col = warn_col;
            fade = 3;
        } else {
            col = elapsed_col;
            fade = 3;
        }

        if (track_is_importing(tr)) {
            col.b >>= 1;
            col.g >>= 1;
            col.r >>= 1;
        }

        if (c < current_position) {
            col.b >>= 1;
            col.g >>= 1;
            col.r >>= 1;
        }

        /* Store a pointer to this column of the framebuffer */

        p = pixels + y * pitch + (x + c) * bytes_per_pixel;

        r = h;
        while (r > height) {
            p[0] = col.b >> fade;
            p[1] = col.g >> fade;
            p[2] = col.r >> fade;
            p += pitch;
            r--;
        }
        while (r) {
            p[0] = col.b;
            p[1] = col.g;
            p[2] = col.r;
            p += pitch;
            r--;
        }
    }
}

/*
 * Draw the close-up meter, which can be zoomed to a level set by
 * 'scale'
 */

static void draw_closeup(SDL_Surface *surface, const struct rect *rect,
                         struct track *tr, int position, int scale)
{
    int x, y, w, h, c;
    size_t bytes_per_pixel, pitch;
    Uint8 *pixels;

    x = rect->x;
    y = rect->y;
    w = rect->w;
    h = rect->h;

    pixels = surface->pixels;
    bytes_per_pixel = surface->format->BytesPerPixel;
    pitch = surface->pitch;

    /* Draw in columns. This may seem like a performance hit,
     * but oprofile shows it makes no difference */

    for (c = 0; c < w; c++) {
        int r, sp, height, fade;
        Uint8 *p;
        SDL_Color col;

        /* Work out the meter height in pixels for this column */

        sp = position - (position % (1 << scale))
            + ((c - w / 2) << scale);

        if (sp < tr->length && sp > 0)
            height = track_get_ppm(tr, sp) * h / 256;
        else
            height = 0;

        /* Select the appropriate colour */

        if (c == w / 2) {
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
        while (r > height) {
            p[0] = col.b >> fade;
            p[1] = col.g >> fade;
            p[2] = col.r >> fade;
            p += pitch;
            r--;
        }
        while (r) {
            p[0] = col.b;
            p[1] = col.g;
            p[2] = col.r;
            p += pitch;
            r--;
        }
    }
}

/*
 * Draw the audio meters for a deck
 */

static void draw_meters(SDL_Surface *surface, const struct rect *rect,
                        struct track *tr, int position, int scale)
{
    struct rect overview, closeup;

    split_top(rect, &overview, &closeup, OVERVIEW_HEIGHT, SPACER);

    if (closeup.h > OVERVIEW_HEIGHT)
        draw_overview(surface, &overview, tr, position);
    else
        closeup = *rect;

    draw_closeup(surface, &closeup, tr, position, scale);
}

/*
 * Draw the current playback status -- clocks, spinner and scope
 */

static void draw_deck_top(SDL_Surface *surface, const struct rect *rect,
                          struct player *pl, struct track *track)
{
    struct rect clocks, left, right, spinner, scope;

    split_left(rect, &clocks, &right, CLOCKS_WIDTH, SPACER);

    /* If there is no timecoder to display information on, or not enough
     * available space, just draw clocks which span the overall space */

    if (!pl->timecode_control || right.w < 0) {
        draw_deck_clocks(surface, rect, pl, track);
        return;
    }

    draw_deck_clocks(surface, &clocks, pl, track);

    split_right(&right, &left, &spinner, SPINNER_SIZE, SPACER);
    if (left.w < 0)
        return;
    split_bottom(&spinner, NULL, &spinner, SPINNER_SIZE, 0);
    draw_spinner(surface, &spinner, pl);

    split_right(&left, &clocks, &scope, SCOPE_SIZE, SPACER);
    if (clocks.w < 0)
        return;
    split_bottom(&scope, NULL, &scope, SCOPE_SIZE, 0);
    draw_scope(surface, &scope, pl->timecoder);
}

/*
 * Draw the textual description of playback status, which includes
 * information on the timecode
 */

static void draw_deck_status(SDL_Surface *surface,
                             const struct rect *rect,
                             const struct deck *deck)
{
    char buf[128], *c;
    int tc;
    const struct player *pl = &deck->player;

    c = buf;

    c += sprintf(c, "%s: ", pl->timecoder->def->name);

    tc = timecoder_get_position(pl->timecoder, NULL);
    if (pl->timecode_control && tc != -1) {
        c += sprintf(c, "%7d ", tc);
    } else {
        c += sprintf(c, "        ");
    }

    sprintf(c, "pitch:%+0.2f (sync %0.2f %+.5fs = %+0.2f)  %s%s",
            pl->pitch,
            pl->sync_pitch,
            pl->last_difference,
            pl->pitch * pl->sync_pitch,
            pl->recalibrate ? "RCAL  " : "",
            deck_is_locked(deck) ? "LOCK  " : "");

    draw_text(surface, rect, buf, detail_font, detail_col, background_col);
}

/*
 * Draw a single deck
 */

static void draw_deck(SDL_Surface *surface, const struct rect *rect,
                      struct deck *deck, int meter_scale)
{
    int position;
    struct rect track, top, meters, status, rest, lower;
    struct player *pl;
    struct track *t;

    pl = &deck->player;
    t = pl->track;

    position = player_get_elapsed(pl) * t->rate;

    split_top(rect, &track, &rest, FONT_SPACE * 2, 0);
    if (rest.h < 160)
        rest = *rect;
    else
        draw_record(surface, &track, deck->record);

    split_top(&rest, &top, &lower, CLOCK_FONT_SIZE * 2, SPACER);
    if (lower.h < 64)
        lower = rest;
    else
        draw_deck_top(surface, &top, pl, t);

    split_bottom(&lower, &meters, &status, FONT_SPACE, SPACER);
    if (meters.h < 64)
        meters = lower;
    else
        draw_deck_status(surface, &status, deck);

    draw_meters(surface, &meters, t, position, meter_scale);
}

/*
 * Draw all the decks in the system left to right
 */

static void draw_decks(SDL_Surface *surface, const struct rect *rect,
                       struct deck deck[], size_t ndecks, int meter_scale)
{
    int d, deck_width;
    struct rect single;

    deck_width = (rect->w - BORDER * (ndecks - 1)) / ndecks;

    single = *rect;
    single.w = deck_width;

    for (d = 0; d < ndecks; d++) {
        single.x = rect->x + (deck_width + BORDER) * d;
        draw_deck(surface, &single, &deck[d], meter_scale);
    }
}

/*
 * Draw the status bar
 */

static void draw_status(SDL_Surface *sf, const struct rect *rect,
                        const char *text)
{
    draw_text(sf, rect, text, detail_font, detail_col, background_col);
}

/*
 * Draw the search field which the user types into
 */

static void draw_search(SDL_Surface *surface, const struct rect *rect,
                        struct selector *sel)
{
    int s;
    const char *buf;
    char cm[32];
    SDL_Rect cursor;
    struct rect rtext;

    split_left(rect, NULL, &rtext, SCROLLBAR_SIZE, SPACER);

    if (sel->search[0] != '\0')
        buf = sel->search;
    else
        buf = NULL;

    s = draw_text(surface, &rtext, buf, font, text_col, background_col);

    cursor.x = rtext.x + s;
    cursor.y = rtext.y;
    cursor.w = CURSOR_WIDTH;
    cursor.h = rtext.h;

    SDL_FillRect(surface, &cursor, palette(surface, &cursor_col));

    if (sel->view_listing->entries > 1)
        sprintf(cm, "%zd matches", sel->view_listing->entries);
    else if (sel->view_listing->entries > 0)
        sprintf(cm, "1 match");
    else
        sprintf(cm, "no matches");

    rtext.x += s + CURSOR_WIDTH + SPACER;
    rtext.w -= s + CURSOR_WIDTH + SPACER;

    draw_text(surface, &rtext, cm, em_font, detail_col, background_col);
}

/*
 * Draw a vertical scroll bar representing our view on a list of the
 * given number of entries
 */

static void draw_scroll_bar(SDL_Surface *surface, const struct rect *rect,
                            const struct scroll *scroll)
{
    SDL_Rect box;
    SDL_Color bg;

    bg = selected_col;
    bg.r >>= 1;
    bg.g >>= 1;
    bg.b >>= 1;

    box.x = rect->x;
    box.y = rect->y;
    box.w = rect->w;
    box.h = rect->h;
    SDL_FillRect(surface, &box, palette(surface, &bg));

    if (scroll->entries > 0) {
        box.x = rect->x;
        box.y = rect->y + rect->h * scroll->offset / scroll->entries;
        box.w = rect->w;
        box.h = rect->h * MIN(scroll->lines, scroll->entries) / scroll->entries;
        SDL_FillRect(surface, &box, palette(surface, &selected_col));
    }
}

/*
 * Display a crate listing, with scrollbar and current selection
 *
 * Return: the number of lines which fit on the display.
 */

static void draw_crates(SDL_Surface *surface, const struct rect *rect,
                        const struct library *library,
                        const struct scroll *scroll)
{
    size_t n;
    struct rect left, bottom;

    split_left(rect, &left, &bottom, SCROLLBAR_SIZE, SPACER);
    draw_scroll_bar(surface, &left, scroll);

    n = scroll->offset;

    for (;;) {
        SDL_Color col_text, col_bg;
        struct rect top;
        const struct crate *crate;

        if (n >= library->crates)
            break;

        if (bottom.h < FONT_SPACE)
            break;

        split_top(&bottom, &top, &bottom, FONT_SPACE, 0);

        crate = library->crate[n];

        col_text = crate->is_fixed ? detail_col: text_col;
        col_bg = (n == scroll->selected) ? selected_col : background_col;

        draw_text(surface, &top, crate->name, font, col_text, col_bg);

        n++;
    }

    draw_rect(surface, &bottom, background_col);
}

/*
 * Display a record library listing, with scrollbar and current
 * selection
 *
 * Return: the number of lines which fit on the display
 */

static void draw_listing(SDL_Surface *surface, const struct rect *rect,
                         const struct listing *listing,
                         const struct scroll *scroll)
{
    size_t n;
    struct rect left, bottom;

    split_left(rect, &left, &bottom, SCROLLBAR_SIZE, SPACER);
    draw_scroll_bar(surface, &left, scroll);

    n = scroll->offset;

    for (;;) {
        SDL_Color col;
        struct rect top, left, right;
        const struct record *record;

        if (n >= listing->entries)
            break;

        if (bottom.h < FONT_SPACE)
            break;

        split_top(&bottom, &top, &bottom, FONT_SPACE, 0);

        record = listing->record[n];

        if (n == scroll->selected) {
            col = selected_col;
        } else {
            col = background_col;
        }

        split_left(&top, &left, &right, RESULTS_ARTIST_WIDTH, 0);
        draw_text(surface, &left, record->artist, font, text_col, col);

        split_left(&right, &left, &right, SPACER, 0);
        draw_rect(surface, &left, col);
        draw_text(surface, &right, record->title, font, text_col, col);

        n++;
    }

    draw_rect(surface, &bottom, background_col);
}

/*
 * Display the music library, which consists of the query, and search
 * results
 */

static void draw_library(SDL_Surface *surface, const struct rect *rect,
                         struct selector *sel)
{
    unsigned int lines;
    struct rect rsearch, rlists, rcrates, rrecords;

    split_top(rect, &rsearch, &rlists, SEARCH_HEIGHT, SPACER);
    draw_search(surface, &rsearch, sel);

    lines = rlists.h / FONT_SPACE;
    selector_set_lines(sel, lines);

    split_left(&rlists, &rcrates, &rrecords, (rlists.w / 4), SPACER);
    if (rcrates.w > LIBRARY_MIN_WIDTH) {
        draw_listing(surface, &rrecords, sel->view_listing, &sel->records);
        draw_crates(surface, &rcrates, sel->library, &sel->crates);
    } else {
        draw_listing(surface, rect, sel->view_listing, &sel->records);
    }
}

/*
 * Handle a single key event
 *
 * Return: true if the selector needs to be redrawn, otherwise false
 */

static bool handle_key(struct deck deck[], size_t ndeck,
                       struct selector *sel, int *meter_scale,
                       SDLKey key, SDLMod mod)
{
    if (key >= SDLK_a && key <= SDLK_z) {
        selector_search_refine(sel, (key - SDLK_a) + 'a');
        return true;

    } else if (key >= SDLK_0 && key <= SDLK_9) {
        selector_search_refine(sel, (key - SDLK_0) + '0');
        return true;

    } else if (key == SDLK_SPACE) {
        selector_search_refine(sel, ' ');
        return true;

    } else if (key == SDLK_BACKSPACE) {
        selector_search_expand(sel);
        return true;

    } else if (key == SDLK_PERIOD) {
        selector_search_refine(sel, '.');
        return true;

    } else if (key == SDLK_HOME) {
        selector_top(sel);
        return true;

    } else if (key == SDLK_END) {
        selector_bottom(sel);
        return true;

    } else if (key == SDLK_UP) {
        selector_up(sel);
        return true;

    } else if (key == SDLK_DOWN) {
        selector_down(sel);
        return true;

    } else if (key == SDLK_PAGEUP) {
        selector_page_up(sel);
        return true;

    } else if (key == SDLK_PAGEDOWN) {
        selector_page_down(sel);
        return true;

    } else if (key == SDLK_LEFT) {
        selector_prev(sel);
        return true;

    } else if (key == SDLK_RIGHT) {
        selector_next(sel);
        return true;

    } else if (key == SDLK_TAB) {
        selector_toggle(sel);
        return true;

    } else if ((key == SDLK_EQUALS) || (key == SDLK_PLUS)) {
        (*meter_scale)--;

        if (*meter_scale < 0)
            *meter_scale = 0;

        fprintf(stderr, "Meter scale decreased to %d\n", *meter_scale);

    } else if (key == SDLK_MINUS) {
        (*meter_scale)++;

        if (*meter_scale > MAX_METER_SCALE)
            *meter_scale = MAX_METER_SCALE;

        fprintf(stderr, "Meter scale increased to %d\n", *meter_scale);

    } else if (key >= SDLK_F1 && key <= SDLK_F12) {
        size_t d;

        /* Handle the function key press in groups of four --
	 * F1-F4 (deck 0), F5-F8 (deck 1) etc. */

        d = (key - SDLK_F1) / 4;

        if (d < ndeck) {
            int func;
            struct deck *de;
            struct player *pl;
            struct record *re;
            struct timecoder *tc;

            func = (key - SDLK_F1) % 4;

            de = &deck[d];
            pl = &de->player;
            tc = &de->timecoder;

            if (mod & KMOD_SHIFT) {
                if (func < ndeck)
                    deck_clone(de, &deck[func]);

            } else switch(func) {
            case FUNC_LOAD:
                re = selector_current(sel);
                if (re != NULL)
                    deck_load(de, re);
                break;

            case FUNC_RECUE:
                deck_recue(de);
                break;

            case FUNC_TIMECODE:
                if (mod & KMOD_CTRL) {
                    timecoder_cycle_definition(tc);
                } else {
                    (void)player_toggle_timecode_control(pl);
                }
                break;
            }
        }
    }

    return false;
}

/*
 * Action on size change event on the main window
 */

static SDL_Surface* set_size(int w, int h, struct rect *rect)
{
    SDL_Surface *surface;

    surface = SDL_SetVideoMode(w, h, 32, SDL_RESIZABLE);
    if (surface == NULL) {
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

/*
 * Timer which posts a screen redraw event
 */

static Uint32 ticker(Uint32 interval, void *p)
{
    SDL_Event event;

    if (!SDL_PeepEvents(&event, 1, SDL_PEEKEVENT, SDL_EVENTMASK(SDL_USEREVENT)))
    {
        event.type = SDL_USEREVENT;
        event.user.code = EVENT_TICKER;
        SDL_PushEvent(&event);
    }

    return interval;
}

/*
 * The SDL interface thread
 */

static int interface_main(void)
{
    int meter_scale, library_update, decks_update, status_update;
    const char *status = banner;

    SDL_Event event;
    SDL_TimerID timer;
    SDL_Surface *surface;

    struct rect rworkspace, rplayers, rlibrary, rstatus, rtmp;

    meter_scale = DEFAULT_METER_SCALE;

    surface = set_size(DEFAULT_WIDTH, DEFAULT_HEIGHT, &rworkspace);
    if (!surface)
        return -1;

    decks_update = UPDATE_REDRAW;
    status_update = UPDATE_REDRAW;
    library_update = UPDATE_REDRAW;

    /* The final action is to add the timer which triggers refresh */

    timer = SDL_AddTimer(REFRESH, ticker, NULL);

    while (SDL_WaitEvent(&event) >= 0) {

        switch(event.type) {
        case SDL_QUIT: /* user request to quit application; eg. window close */
            if (rig_quit() == -1)
                return -1;
            break;

        case SDL_VIDEORESIZE:
            surface = set_size(event.resize.w, event.resize.h, &rworkspace);
            if (!surface)
                return -1;

            library_update = UPDATE_REDRAW;
            decks_update = UPDATE_REDRAW;
            status_update = UPDATE_REDRAW;

            break;

        case SDL_USEREVENT:
            switch (event.user.code) {
            case EVENT_TICKER: /* request to poll the clocks */
                decks_update = UPDATE_REDRAW;
                break;

            case EVENT_QUIT: /* internal request to finish this thread */
                goto finish;

            default:
                abort();
            }
            break;

        case SDL_KEYDOWN:
            if (handle_key(deck, ndeck, &selector, &meter_scale,
                           event.key.keysym.sym, event.key.keysym.mod))
            {
                library_update = UPDATE_REDRAW;
            }

        } /* switch(event.type) */

        /* Split the display into the various areas. If an area is too
         * small, abandon any actions to happen in that area. */

        split_bottom(&rworkspace, &rtmp, &rstatus, STATUS_HEIGHT, SPACER);
        if (rtmp.h < 128 || rtmp.w < 0) {
            rtmp = rworkspace;
            status_update = UPDATE_NONE;
        }

        split_top(&rtmp, &rplayers, &rlibrary, PLAYER_HEIGHT, SPACER);
        if (rlibrary.h < LIBRARY_MIN_HEIGHT || rlibrary.w < LIBRARY_MIN_WIDTH) {
            rplayers = rtmp;
            library_update = UPDATE_NONE;
        }

        if (rplayers.h < 0 || rplayers.w < 0)
            decks_update = UPDATE_NONE;

        /* If there's been a change to the library search results,
         * check them over and display them. */

        if (library_update == UPDATE_REDRAW) {

            if (selector_current(&selector) != NULL)
                status = selector_current(&selector)->pathname;
            else
                status = "No search results found";
            status_update = UPDATE_REDRAW;

            LOCK(surface);
            draw_library(surface, &rlibrary, &selector);
            UNLOCK(surface);
            UPDATE(surface, &rlibrary);
            library_update = UPDATE_NONE;
        }

        /* If there's been a change to status, redisplay it */

        if (status_update == UPDATE_REDRAW) {
            LOCK(surface);
            draw_status(surface, &rstatus, status);
            UNLOCK(surface);
            UPDATE(surface, &rstatus);
            status_update = UPDATE_NONE;
        }

        /* If it's due, redraw the players. This is triggered by the
         * timer event. */

        if (decks_update == UPDATE_REDRAW) {
            LOCK(surface);
            draw_decks(surface, &rplayers, deck, ndeck, meter_scale);
            UNLOCK(surface);
            UPDATE(surface, &rplayers);
            decks_update = UPDATE_NONE;
        }

    } /* main loop */

 finish:
    SDL_RemoveTimer(timer);

    return 0;
}

static void* launch(void *p)
{
    interface_main();
    return NULL;
}

/*
 * Start the SDL interface
 *
 * FIXME: There are multiple points where resources are leaked on
 * error
 */

int interface_start(struct deck ldeck[], size_t lndeck, struct library *lib)
{
    size_t n;

    deck = ldeck;
    ndeck = lndeck;

    for (n = 0; n < ndeck; n++) {
        if (timecoder_monitor_init(&deck[n].timecoder, SCOPE_SIZE) == -1)
            return -1;
    }

    selector_init(&selector, lib);
    calculate_spinner_lookup(spinner_angle, NULL, SPINNER_SIZE);

    fprintf(stderr, "Initialising SDL...\n");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == -1) {
        fprintf(stderr, "%s\n", SDL_GetError());
        return -1;
    }
    SDL_WM_SetCaption(banner, NULL);
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

    /* Initialise the fonts */

    if (TTF_Init() == -1) {
        fprintf(stderr, "%s\n", TTF_GetError());
        return -1;
    }

    if (load_fonts() == -1)
        return -1;

    fprintf(stderr, "Launching interface thread...\n");

    if (pthread_create(&ph, NULL, launch, NULL)) {
        perror("pthread_create");
        return -1;
    }

    return 0;
}

/*
 * Synchronise with the SDL interface and exit
 */

void interface_stop(void)
{
    size_t n;
    SDL_Event quit;

    quit.type = SDL_USEREVENT;
    quit.user.code = EVENT_QUIT;
    if (SDL_PushEvent(&quit) == -1)
        abort();

    if (pthread_join(ph, NULL) != 0)
        abort();

    for (n = 0; n < ndeck; n++)
        timecoder_monitor_clear(&deck[n].timecoder);

    selector_clear(&selector);

    clear_fonts();

    TTF_Quit();
    SDL_Quit();
}
