// demo_list.c - exercises the mosquito list widget. Build with build.sh.
#define MSQ_IMPLEMENTATION
#include "mosquito.h"
#include <stdio.h>
int main (int argc, char **argv)
{
  (void) argc; (void) argv;
  if (SDL_Init (SDL_INIT_VIDEO) != 0)
    {
      fprintf (stderr, "SDL_Init: %s\n", SDL_GetError ());
      return 1;
    }
#ifndef MSQ_NO_TTF
  TTF_Init ();
#endif
  SDL_Window *win = SDL_CreateWindow ("mosquito list demo",
                                      SDL_WINDOWPOS_CENTERED,
                                      SDL_WINDOWPOS_CENTERED, 480, 460,
                                      SDL_WINDOW_SHOWN);
  SDL_Renderer *ren = SDL_CreateRenderer (win, -1, SDL_RENDERER_ACCELERATED);
  // try a few common font paths; falls back to NULL (blank text) if missing
  const char *fonts[] = {
    "./Roboto-Medium.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    NULL
  };
  const char *fpath = NULL;
  for (int i = 0; fonts[i]; i++)
    {
      FILE *f = fopen (fonts[i], "rb");
      if (f) { fpath = fonts[i]; fclose (f); break; }
    }
  MsqCtx *ui = msq_create (ren, fpath, 13);

  // a list with more items than visible rows, so it scrolls
  const char *fruits[] = {
    "Apple", "Banana", "Cherry", "Date", "Elderberry",
    "Fig", "Grape", "Honeydew", "Kiwi", "Lemon",
    "Mango", "Nectarine", "Orange", "Papaya", "Quince"
  };
  int fruit_count = (int) (sizeof fruits / sizeof *fruits);
  int sel = 0;

  // a short list that fits entirely (no scrollbar expected)
  const char *modes[] = { "Draw", "Erase", "Select", "Pan" };
  int mode_sel = 1;

  bool running = true;
  while (running)
    {
      SDL_Event e;
      msq_begin_frame (ui);
      while (SDL_PollEvent (&e))
        {
          if (e.type == SDL_QUIT)
            running = false;
          msq_handle_event (ui, &e);
        }
      MsqColor bg = msq_color (MSQ_COL_WINDOW_BG);
      SDL_SetRenderDrawColor (ren, bg.r, bg.g, bg.b, bg.a);
      SDL_RenderClear (ren);

      msq_panel_begin (ui, 16, 16, 448);
      msq_label (ui, "mosquito list tour");
      msq_spacer (ui, 6);

      // scrolling list: 6 rows visible, wheel to scroll the rest
      if (msq_list (ui, "Fruit (scrolls)", fruits, fruit_count, &sel, 6))
        printf ("fruit selected: %s\n", fruits[sel]);

      char sbuf[64];
      SDL_snprintf (sbuf, sizeof sbuf, "Selected: %s", fruits[sel]);
      msq_label (ui, sbuf);
      msq_spacer (ui, 6);

      // short list that fits, no scrollbar
      if (msq_list (ui, "Mode (fits)", modes, 4, &mode_sel, 0))
        printf ("mode selected: %s\n", modes[mode_sel]);

      msq_panel_end (ui);
      msq_end_frame (ui);
      SDL_RenderPresent (ren);
      SDL_Delay (16);
    }
  msq_destroy (ui);
  SDL_DestroyRenderer (ren);
  SDL_DestroyWindow (win);
#ifndef MSQ_NO_TTF
  TTF_Quit ();
#endif
  SDL_Quit ();
  return 0;
}
