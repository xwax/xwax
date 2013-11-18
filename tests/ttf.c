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

    SDL_UpdateRect(surface, 0, 0, source.w, source.h);
}

int main(int argc, char *argv[])
{
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

    surface = SDL_SetVideoMode(400, 200, 32, 0);
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
    }
done:

    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
