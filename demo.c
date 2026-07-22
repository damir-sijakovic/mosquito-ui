// demo.c - exercises every mosquito widget. Build with build.sh.
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

  SDL_Window *win = SDL_CreateWindow ("mosquito demo", SDL_WINDOWPOS_CENTERED,
                                      SDL_WINDOWPOS_CENTERED, 480, 420,
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

  bool checked = true;
  float vol = 0.4f;
  char name[MSQ_TEXT_CAP] = "mosquito";
  int dd_sel = 1;
  const char *modes[] = { "Draw", "Erase", "Select", "Pan" };
  MsqId ctxmenu = msq_gen_id (ui, "canvas-context-menu");
  const char *ctxitems[] = { "Cut", "Copy", "Paste", "Delete" };
  int click_count = 0;

  bool running = true;
  while (running)
    {
      SDL_Event e;
      msq_begin_frame (ui);
      while (SDL_PollEvent (&e))
        {
          if (e.type == SDL_QUIT)
            running = false;
          if (e.type == SDL_MOUSEBUTTONDOWN
              && e.button.button == SDL_BUTTON_RIGHT)
            msq_popup_open (ui, ctxmenu, e.button.x, e.button.y);
          msq_handle_event (ui, &e);
        }

      MsqColor bg = msq_color (MSQ_COL_WINDOW_BG);
      SDL_SetRenderDrawColor (ren, bg.r, bg.g, bg.b, bg.a);
      SDL_RenderClear (ren);

      msq_panel_begin (ui, 16, 16, 448);
      msq_label (ui, "mosquito widget tour");
      msq_spacer (ui, 6);

      if (msq_button (ui, "Click me"))
        click_count++;
      msq_tooltip (ui, "increments the counter");

      char cbuf[64];
      SDL_snprintf (cbuf, sizeof cbuf, "Clicks: %d", click_count);
      msq_label (ui, cbuf);

      msq_checkbox (ui, "Enable grid snapping", &checked);
      msq_tooltip (ui, "toggles snap-to-grid");

      msq_slider (ui, "Volume", &vol, 0.f, 1.f);

      msq_text_input (ui, "Project name", name, MSQ_TEXT_CAP);

      msq_dropdown (ui, "Tool", modes, 4, &dd_sel);
      msq_tooltip (ui, "right-click the window for a popup menu");

      msq_row_begin (ui);
      if (msq_button (ui, "OK")) running = running;
      msq_button (ui, "Cancel");
      msq_row_end (ui);

      msq_panel_end (ui);

      int choice;
      if (msq_popup (ui, ctxmenu, ctxitems, 4, &choice))
        printf ("context menu chose: %s\n", ctxitems[choice]);

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
