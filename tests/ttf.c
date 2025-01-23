/*
 * Copyright (C) 2025 Mark Hills <mark@xwax.org>
 *
 * This file is part of "xwax".
 *
 * "xwax" is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * "xwax" is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */

/*
 * Illustrate clipping of characters
 */

#include <stdio.h>
#include <SDL.h>
#include <SDL_ttf.h>

SDL_Surface *surface;
TTF_Font *font;

void draw(void)
{
    const SDL_Color fg = { 255, 255, 255, 255 }, bg = { 0, 0, 0, 255 };
    SDL_Surface *rendered;
    SDL_Rect dest, source;

    rendered = TTF_RenderText_Shaded(font, "Track at 101.0 BPM a0a0", fg, bg);

    source.x = 0;
    source.y = 0;
    source.w = rendered->w;
    source.h = rendered->h;

    dest.x = 0;
    dest.y = 0;

    SDL_BlitSurface(rendered, &source, surface, &dest);
    SDL_FreeSurface(rendered);
}

int main(int argc, char *argv[])
{
    SDL_Window *window;
    SDL_Renderer *renderer;

    if (argc != 2) {
        fputs("usage: test-ttf <font-file>\n", stderr);
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) == -1)
        abort();

    if (TTF_Init() == -1)
        abort();

    font = TTF_OpenFont(argv[1], 15);
    if (font == NULL)
        abort();

#ifdef TTF_HINTING_NONE
    TTF_SetFontHinting(font, TTF_HINTING_NONE);
#endif
    TTF_SetFontKerning(font, 1);

    window = SDL_CreateWindow(argv[0],
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              400, 200, 0);

    renderer = SDL_CreateRenderer(window, -1, 0);
    if (renderer == NULL)
        abort();

    surface = SDL_GetWindowSurface(window);
    if (surface == NULL)
        abort();

    for (;;) {
        SDL_Event event;

        if (SDL_WaitEvent(&event) < 0)
            abort();

        switch (event.type) {
        case SDL_QUIT:
            goto done;
        }

        draw();
        SDL_UpdateWindowSurface(window);
    }
done:

    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
