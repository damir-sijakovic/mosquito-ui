// gcc -o child_windows main.c `pkg-config --cflags --libs sdl2` -Wall -Wextra

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── tuneable constants ────────────────────────────────────────────────────── */
#define APP_W          1280
#define APP_H          720
#define TITLEBAR_H     28
#define CLOSE_BTN_SIZE 18
#define CLOSE_BTN_PAD  5          /* distance from right edge of titlebar */
#define MAX_TITLE      128

/* colours */
#define COL_TITLEBAR    0x2C3E50FF
#define COL_TITLE_HOVER 0x34495EFF
#define COL_CLOSE       0xE74C3CFF
#define COL_CLOSE_HOVER 0xFF6B6BFF
#define COL_BORDER      0x1A252FFF
#define COL_BODY        0xECF0F1FF
#define COL_TEXT        0xFFFFFFFF
#define COL_BG          0x1ABC9CFF   /* main window background */

/* ── structs ───────────────────────────────────────────────────────────────── */
typedef struct {
    /* geometry */
    SDL_Rect rect;           /* full window rect (includes titlebar) */

    /* state */
    int      visible;
    int      dragging;
    int      drag_off_x;    /* offset from top-left while dragging */
    int      drag_off_y;

    /* meta */
    char     title[MAX_TITLE];
    int      id;
} ChildWindow;

typedef struct {
    SDL_Window   *sdl_win;
    SDL_Renderer *renderer;

    ChildWindow **windows;   /* dynamic array */
    int           count;
    int           capacity;

    int           next_id;
} App;

/* ── helpers ───────────────────────────────────────────────────────────────── */
static void set_color_hex(SDL_Renderer *r, Uint32 hex)
{
    SDL_SetRenderDrawColor(r,
        (hex >> 24) & 0xFF,
        (hex >> 16) & 0xFF,
        (hex >>  8) & 0xFF,
        (hex      ) & 0xFF);
}

static SDL_Rect close_btn_rect(const ChildWindow *cw)
{
    return (SDL_Rect){
        cw->rect.x + cw->rect.w - CLOSE_BTN_SIZE - CLOSE_BTN_PAD,
        cw->rect.y + (TITLEBAR_H - CLOSE_BTN_SIZE) / 2,
        CLOSE_BTN_SIZE,
        CLOSE_BTN_SIZE
    };
}

static SDL_Rect titlebar_rect(const ChildWindow *cw)
{
    return (SDL_Rect){ cw->rect.x, cw->rect.y, cw->rect.w, TITLEBAR_H };
}

static int point_in_rect(int x, int y, SDL_Rect r)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

/* ── draw a simple bitmap 'X' (no font needed) ─────────────────────────────── */
static void draw_x(SDL_Renderer *r, SDL_Rect btn)
{
    int m = 4; /* margin inside button */
    int x1 = btn.x + m, y1 = btn.y + m;
    int x2 = btn.x + btn.w - m - 1, y2 = btn.y + btn.h - m - 1;
    for (int t = -1; t <= 1; t++) {
        SDL_RenderDrawLine(r, x1, y1 + t, x2, y2 + t);
        SDL_RenderDrawLine(r, x1, y2 + t, x2, y1 + t);
    }
}

/* ── draw a simple bitmap title string (3×5 pixel font, ASCII 32-90) ────────
   This tiny font avoids the SDL_ttf dependency entirely.                      */
static const Uint8 FONT3x5[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* ' ' */
    {0x40,0x40,0x40,0x00,0x40}, /* '!' */
    {0xA0,0xA0,0x00,0x00,0x00}, /* '"' */
    {0xA0,0xE0,0xA0,0xE0,0xA0}, /* '#' */
    {0x60,0xC0,0x60,0xC0,0x60}, /* '$' – placeholder */
    {0x80,0x20,0x40,0x80,0x20}, /* '%' – placeholder */
    {0x40,0xA0,0x40,0xA0,0xC0}, /* '&' – placeholder */
    {0x40,0x40,0x00,0x00,0x00}, /* '\'' */
    {0x20,0x40,0x40,0x40,0x20}, /* '(' */
    {0x80,0x40,0x40,0x40,0x80}, /* ')' */
    {0xA0,0x40,0xE0,0x40,0xA0}, /* '*' */
    {0x00,0x40,0xE0,0x40,0x00}, /* '+' */
    {0x00,0x00,0x00,0x40,0x80}, /* ',' */
    {0x00,0x00,0xE0,0x00,0x00}, /* '-' */
    {0x00,0x00,0x00,0x00,0x40}, /* '.' */
    {0x20,0x20,0x40,0x80,0x80}, /* '/' */
    {0x60,0xA0,0xA0,0xA0,0x60}, /* '0' */
    {0x40,0xC0,0x40,0x40,0xE0}, /* '1' */
    {0xC0,0x20,0x40,0x80,0xE0}, /* '2' */
    {0xC0,0x20,0x40,0x20,0xC0}, /* '3' */
    {0xA0,0xA0,0xE0,0x20,0x20}, /* '4' */
    {0xE0,0x80,0xC0,0x20,0xC0}, /* '5' */
    {0x60,0x80,0xE0,0xA0,0x60}, /* '6' */
    {0xE0,0x20,0x40,0x80,0x80}, /* '7' */
    {0x60,0xA0,0x60,0xA0,0x60}, /* '8' */
    {0x60,0xA0,0x60,0x20,0x60}, /* '9' */
    {0x00,0x40,0x00,0x40,0x00}, /* ':' */
    {0x00,0x40,0x00,0x40,0x80}, /* ';' */
    {0x20,0x40,0x80,0x40,0x20}, /* '<' */
    {0x00,0xE0,0x00,0xE0,0x00}, /* '=' */
    {0x80,0x40,0x20,0x40,0x80}, /* '>' */
    {0xC0,0x20,0x40,0x00,0x40}, /* '?' */
    {0x60,0xA0,0xE0,0x80,0x60}, /* '@' – placeholder */
    {0x40,0xA0,0xE0,0xA0,0xA0}, /* 'A' */
    {0xC0,0xA0,0xC0,0xA0,0xC0}, /* 'B' */
    {0x60,0x80,0x80,0x80,0x60}, /* 'C' */
    {0xC0,0xA0,0xA0,0xA0,0xC0}, /* 'D' */
    {0xE0,0x80,0xC0,0x80,0xE0}, /* 'E' */
    {0xE0,0x80,0xC0,0x80,0x80}, /* 'F' */
    {0x60,0x80,0xA0,0xA0,0x60}, /* 'G' */
    {0xA0,0xA0,0xE0,0xA0,0xA0}, /* 'H' */
    {0xE0,0x40,0x40,0x40,0xE0}, /* 'I' */
    {0x20,0x20,0x20,0xA0,0x40}, /* 'J' */
    {0xA0,0xA0,0xC0,0xA0,0xA0}, /* 'K' */
    {0x80,0x80,0x80,0x80,0xE0}, /* 'L' */
    {0xA0,0xE0,0xE0,0xA0,0xA0}, /* 'M' */
    {0xA0,0xE0,0xE0,0xC0,0xA0}, /* 'N' */
    {0x40,0xA0,0xA0,0xA0,0x40}, /* 'O' */
    {0xC0,0xA0,0xC0,0x80,0x80}, /* 'P' */
    {0x40,0xA0,0xA0,0xC0,0x60}, /* 'Q' */
    {0xC0,0xA0,0xC0,0xA0,0xA0}, /* 'R' */
    {0x60,0x80,0x40,0x20,0xC0}, /* 'S' */
    {0xE0,0x40,0x40,0x40,0x40}, /* 'T' */
    {0xA0,0xA0,0xA0,0xA0,0x60}, /* 'U' */
    {0xA0,0xA0,0xA0,0x40,0x40}, /* 'V' */
    {0xA0,0xA0,0xE0,0xE0,0xA0}, /* 'W' */
    {0xA0,0xA0,0x40,0xA0,0xA0}, /* 'X' */
    {0xA0,0xA0,0x40,0x40,0x40}, /* 'Y' */
    {0xE0,0x20,0x40,0x80,0xE0}, /* 'Z' */
};

static void draw_char(SDL_Renderer *r, int cx, int cy, char ch, int scale)
{
    int idx = (int)ch - 32;
    if (idx < 0 || idx >= (int)(sizeof FONT3x5 / sizeof FONT3x5[0])) return;
    const Uint8 *glyph = FONT3x5[idx];
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 3; col++) {
            if (glyph[row] & (0x80 >> col)) {
                SDL_Rect px = { cx + col*scale, cy + row*scale, scale, scale };
                SDL_RenderFillRect(r, &px);
            }
        }
    }
}

static void draw_string(SDL_Renderer *r, int x, int y, const char *s, int scale)
{
    for (int i = 0; s[i]; i++)
        draw_char(r, x + i * (3*scale + scale), y, s[i] >= 'a' && s[i] <= 'z'
                  ? s[i] - 32 : s[i], scale);
}

/* ── ChildWindow lifecycle ──────────────────────────────────────────────────── */
ChildWindow *create_child_window(App *app,
                                 const char *title,
                                 int x, int y,
                                 int w, int h)
{
    /* grow the vector if needed */
    if (app->count >= app->capacity) {
        app->capacity = app->capacity ? app->capacity * 2 : 4;
        app->windows  = realloc(app->windows,
                                app->capacity * sizeof(ChildWindow *));
        if (!app->windows) { fprintf(stderr, "OOM\n"); exit(1); }
    }

    ChildWindow *cw = calloc(1, sizeof(ChildWindow));
    if (!cw) { fprintf(stderr, "OOM\n"); exit(1); }

    cw->rect    = (SDL_Rect){ x, y, w, h };
    cw->visible = 1;
    cw->id      = app->next_id++;
    snprintf(cw->title, MAX_TITLE, "%s", title);

    app->windows[app->count++] = cw;
    return cw;
}

static void destroy_child_window(App *app, int idx)
{
    free(app->windows[idx]);
    /* shift remaining pointers left */
    for (int i = idx; i < app->count - 1; i++)
        app->windows[i] = app->windows[i + 1];
    app->count--;
}

/* ── render one child window ────────────────────────────────────────────────── */
static void render_child_window(App *app, ChildWindow *cw,
                                 int mx, int my)
{
    SDL_Renderer *r = app->renderer;

    /* body */
    set_color_hex(r, COL_BODY);
    SDL_Rect body = { cw->rect.x, cw->rect.y + TITLEBAR_H,
                      cw->rect.w, cw->rect.h - TITLEBAR_H };
    SDL_RenderFillRect(r, &body);

    /* border */
    set_color_hex(r, COL_BORDER);
    SDL_RenderDrawRect(r, &cw->rect);

    /* titlebar */
    SDL_Rect tb = titlebar_rect(cw);
    int hovering_tb = point_in_rect(mx, my, tb);
    set_color_hex(r, hovering_tb ? COL_TITLE_HOVER : COL_TITLEBAR);
    SDL_RenderFillRect(r, &tb);

    /* title text */
    set_color_hex(r, COL_TEXT);
    draw_string(r, tb.x + 6, tb.y + (TITLEBAR_H - 10) / 2, cw->title, 2);

    /* close button */
    SDL_Rect cb = close_btn_rect(cw);
    int hovering_cb = point_in_rect(mx, my, cb);
    set_color_hex(r, hovering_cb ? COL_CLOSE_HOVER : COL_CLOSE);
    SDL_RenderFillRect(r, &cb);

    /* 'X' on close button */
    set_color_hex(r, COL_TEXT);
    draw_x(r, cb);
}

/* ── bring a window to front (move to end of array = drawn last) ─────────────*/
static void bring_to_front(App *app, int idx)
{
    if (idx == app->count - 1) return;
    ChildWindow *cw = app->windows[idx];
    for (int i = idx; i < app->count - 1; i++)
        app->windows[i] = app->windows[i + 1];
    app->windows[app->count - 1] = cw;
}

/* ── App lifecycle ──────────────────────────────────────────────────────────── */
App *create_app(const char *title, int w, int h)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return NULL;
    }

    App *app = calloc(1, sizeof(App));
    app->sdl_win = SDL_CreateWindow(title,
                                    SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED,
                                    w, h,
                                    SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!app->sdl_win) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        free(app); return NULL;
    }

    app->renderer = SDL_CreateRenderer(app->sdl_win, -1,
                                       SDL_RENDERER_ACCELERATED |
                                       SDL_RENDERER_PRESENTVSYNC);
    if (!app->renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(app->sdl_win);
        free(app); return NULL;
    }
    return app;
}

static void destroy_app(App *app)
{
    for (int i = 0; i < app->count; i++) free(app->windows[i]);
    free(app->windows);
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->sdl_win);
    SDL_Quit();
    free(app);
}

/* ── main loop ──────────────────────────────────────────────────────────────── */
int main(void)
{
    App *app = create_app("SDL2 Child Windows Demo", APP_W, APP_H);
    if (!app) return 1;

    /* create a few demo child windows */
    create_child_window(app, "INSPECTOR",  40,  60, 300, 200);
    create_child_window(app, "PROPERTIES",380,  80, 260, 240);
    create_child_window(app, "CONSOLE",    80, 320, 460, 200);
    create_child_window(app, "SCENE",     600, 160, 340, 260);

    int running = 1;
    SDL_Event ev;

    while (running) {
        int mx, my;
        SDL_GetMouseState(&mx, &my);

        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) { running = 0; break; }

            /* keyboard shortcut: N = new child window */
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_n) {
                char name[32];
                snprintf(name, sizeof name, "WINDOW %d", app->next_id);
                int rx = 50 + (app->next_id * 37) % 600;
                int ry = 50 + (app->next_id * 53) % 400;
                create_child_window(app, name, rx, ry, 220, 160);
            }

            if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
                int px = ev.button.x, py = ev.button.y;

                /* iterate back-to-front so topmost window wins */
                for (int i = app->count - 1; i >= 0; i--) {
                    ChildWindow *cw = app->windows[i];
                    if (!cw->visible) continue;

                    SDL_Rect cb = close_btn_rect(cw);
                    SDL_Rect tb = titlebar_rect(cw);

                    if (point_in_rect(px, py, cb)) {
                        /* close */
                        destroy_child_window(app, i);
                        break;
                    }
                    if (point_in_rect(px, py, tb)) {
                        /* start drag */
                        bring_to_front(app, i);
                        cw = app->windows[app->count - 1];
                        cw->dragging  = 1;
                        cw->drag_off_x = px - cw->rect.x;
                        cw->drag_off_y = py - cw->rect.y;
                        break;
                    }
                    if (point_in_rect(px, py, cw->rect)) {
                        bring_to_front(app, i);
                        break;
                    }
                }
            }

            if (ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_LEFT) {
                for (int i = 0; i < app->count; i++)
                    app->windows[i]->dragging = 0;
            }

            if (ev.type == SDL_MOUSEMOTION) {
                for (int i = 0; i < app->count; i++) {
                    ChildWindow *cw = app->windows[i];
                    if (cw->dragging) {
                        cw->rect.x = ev.motion.x - cw->drag_off_x;
                        cw->rect.y = ev.motion.y - cw->drag_off_y;
                    }
                }
            }
        }

        /* ── render ── */
        set_color_hex(app->renderer, COL_BG);
        SDL_RenderClear(app->renderer);

        /* hint text */
        set_color_hex(app->renderer, 0x0D8A6AFF);
        draw_string(app->renderer, 10, 10, "PRESS N FOR NEW WINDOW", 1);

        for (int i = 0; i < app->count; i++)
            if (app->windows[i]->visible)
                render_child_window(app, app->windows[i], mx, my);

        SDL_RenderPresent(app->renderer);
    }

    destroy_app(app);
    return 0;
}
