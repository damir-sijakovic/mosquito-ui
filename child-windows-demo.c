// child-windows-demo.c - two draggable child windows (imgui-style):
//   * "Widgets" holds the demo.c widget tour
//   * "Lists"   holds the demo_list.c list tour
// A small "Panels" window toggles the others and switches the widget font
// between smooth TTF (the default) and the crisp bitmap "rect" font -- the rect
// font is meant only for genuinely tiny captions, like the hint line up top.
// Build with build.sh.
//
// Drag a window by its title bar, drag the bottom-right grip to resize it, click
// the red X to close it, and click any window to bring it to the front.
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

  SDL_Window *win = SDL_CreateWindow ("mosquito child-windows demo",
                                      SDL_WINDOWPOS_CENTERED,
                                      SDL_WINDOWPOS_CENTERED, 1000, 600,
                                      SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  SDL_Renderer *ren = SDL_CreateRenderer (win, -1, SDL_RENDERER_ACCELERATED);

  // try a few common font paths; falls back to NULL (rect font) if missing
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

  /* ---- "Widgets" window state (from demo.c) ---- */
  bool  checked = true;
  float vol = 0.4f;
  char  name[MSQ_TEXT_CAP] = "mosquito";
  int   dd_sel = 1;
  const char *modes[] = { "Draw", "Erase", "Select", "Pan" };
  MsqId ctxmenu = msq_gen_id (ui, "canvas-context-menu");
  const char *ctxitems[] = { "Cut", "Copy", "Paste", "Delete" };
  int   click_count = 0;

  /* ---- "Lists" window state (from demo_list.c) ---- */
  const char *fruits[] = {
    "Apple", "Banana", "Cherry", "Date", "Elderberry",
    "Fig", "Grape", "Honeydew", "Kiwi", "Lemon",
    "Mango", "Nectarine", "Orange", "Papaya", "Quince"
  };
  int fruit_count = (int) (sizeof fruits / sizeof *fruits);
  int sel = 0;
  const char *lmodes[] = { "Draw", "Erase", "Select", "Pan" };
  int mode_sel = 1;

  /* ---- "Notes" window state (textarea) ---- */
  char notes[1024] =
    "Type here.\n"
    "Select with Shift+arrows or the mouse.\n"
    "Ctrl+C copy, Ctrl+X cut, Ctrl+V paste, Ctrl+A all.";

  /* ---- "Tabs" window state ---- */
  const char *pages[] = { "Info", "Style", "About" };
  int  page = 0;
  bool wireframe = false;
  float gamma = 1.0f;

  /* ---- control state ---- */
  bool show_widgets = true;
  bool show_lists   = true;
  bool show_notes   = true;
  bool show_tabs    = true;
  bool rect_font    = false;

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

      /* a top-level hint, drawn straight with the rect font (no widget) */
      msq_rect_text (ren, 12, 12,
                     "DRAG TITLE BARS - X CLOSES - RIGHT-CLICK FOR MENU", 1,
                     msq_color (MSQ_COL_TEXT_DIM));

      /* window 1: the widget tour */
      if (msq_window_begin (ui, "Widgets", 40, 60, 320, 360, &show_widgets))
        {
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
          msq_tooltip (ui, "right-click anywhere for a popup menu");

          msq_row_begin (ui);
          msq_button (ui, "OK");
          msq_button (ui, "Cancel");
          msq_row_end (ui);

          msq_window_end (ui);
        }

      /* window 2: the list tour */
      if (msq_window_begin (ui, "Lists", 400, 90, 320, 400, &show_lists))
        {
          if (msq_list (ui, "Fruit (scrolls)", fruits, fruit_count, &sel, 6))
            printf ("fruit selected: %s\n", fruits[sel]);

          char sbuf[64];
          SDL_snprintf (sbuf, sizeof sbuf, "Selected: %s", fruits[sel]);
          msq_label (ui, sbuf);
          msq_spacer (ui, 6);

          if (msq_list (ui, "Mode (fits)", lmodes, 4, &mode_sel, 0))
            printf ("mode selected: %s\n", lmodes[mode_sel]);

          msq_window_end (ui);
        }

      /* window 3: a multi-line textarea with selection + clipboard */
      if (msq_window_begin (ui, "Notes", 560, 320, 400, 230, &show_notes))
        {
          msq_textarea (ui, "Scratch", notes, sizeof notes, 6);
          msq_tooltip (ui, "Shift+arrows / mouse to select, Ctrl+C/X/V/A");
          msq_window_end (ui);
        }

      /* window 4: a tab group switching between three content pages */
      if (msq_window_begin (ui, "Tabs", 40, 440, 320, 150, &show_tabs))
        {
          msq_tabs (ui, NULL, pages, 3, &page);
          if (page == 0)
            {
              msq_label (ui, "Info: an immediate-mode tab group.");
              msq_checkbox (ui, "Wireframe", &wireframe);
            }
          else if (page == 1)
            {
              msq_slider (ui, "Gamma", &gamma, 0.5f, 2.5f);
              msq_label (ui, "Style settings live here.");
            }
          else
            {
              msq_label (ui, "mosquito tabs");
              msq_label (ui, "click a tab to switch pages.");
            }
          msq_window_end (ui);
        }

      /* window 5: controls -- toggles the other windows and the font */
      if (msq_window_begin (ui, "Panels", 760, 60, 210, 215, NULL))
        {
          msq_checkbox (ui, "Show Widgets", &show_widgets);
          msq_checkbox (ui, "Show Lists",   &show_lists);
          msq_checkbox (ui, "Show Notes",   &show_notes);
          msq_checkbox (ui, "Show Tabs",    &show_tabs);
          msq_spacer (ui, 6);
          if (msq_checkbox (ui, "Rect font (bitmap)", &rect_font))
            msq_set_font (ui, rect_font ? MSQ_FONT_RECT : MSQ_FONT_TTF);
          msq_tooltip (ui, "crisp tiny font, no blur");
          msq_window_end (ui);
        }

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
