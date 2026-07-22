/* mosquito.h - a tiny single-header immediate-mode GUI for SDL2.
 *
 * One header, no .c file, no external deps besides SDL2 (+SDL2_ttf for text).
 * GIMP-style naming: every public symbol is msq_*, every type is MsqFoo,
 * every enum value is MSQ_FOO.
 *
 *   #define MSQ_IMPLEMENTATION
 *   #include "mosquito.h"
 *
 * in exactly ONE translation unit. Include it (without the define) anywhere
 * else you need the declarations.
 *
 * The "innovative" part: mosquito is immediate-mode (you re-declare the whole
 * UI every frame, no widget objects to allocate or free) but it keeps just
 * enough retained state in a single MsqCtx to drive things that genuinely need
 * memory across frames -- focus, hot/active tracking, open popups, tooltip
 * timers, text-input caret. Widget identity is derived from the source line +
 * a salt, so two buttons on the same line still get distinct IDs via msq_push_id.
 *
 * Widgets: button, checkbox, slider, text input, list, popup menu, dropdown,
 * tooltip. Auto stack layout (rows/columns) so you rarely pass coordinates.
 *
 * License: 0BSD / public-domain-equivalent. Do whatever you want.
 */

#ifndef MOSQUITO_H
#define MOSQUITO_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Configuration knobs (override before including with MSQ_IMPLEMENTATION).   */
/* -------------------------------------------------------------------------- */

#ifndef MSQ_MAX_POPUP_ITEMS
#define MSQ_MAX_POPUP_ITEMS 64
#endif

#ifndef MSQ_TEXT_CAP
#define MSQ_TEXT_CAP 256   // capacity of a single text-input buffer the user owns
#endif

#ifndef MSQ_ID_STACK
#define MSQ_ID_STACK 32
#endif

/* Child-window + deferred-draw limits. A window records its drawing into a
 * small command buffer so overlapping windows can be re-layered by z-order. */
#ifndef MSQ_MAX_WINDOWS
#define MSQ_MAX_WINDOWS 32
#endif

#ifndef MSQ_MAX_DRAW_CMDS
#define MSQ_MAX_DRAW_CMDS 2048
#endif

#ifndef MSQ_DRAW_STR_ARENA
#define MSQ_DRAW_STR_ARENA 8192
#endif

/* Use SDL2_ttf for crisp text. If you do not want the dependency, define
 * MSQ_NO_TTF before the implementation and a chunky 5x7 bitmap font is used. */
#ifndef MSQ_NO_TTF
#include <SDL2/SDL_ttf.h>
#endif

/* -------------------------------------------------------------------------- */
/*  Basic value types.                                                         */
/* -------------------------------------------------------------------------- */

typedef uint32_t MsqId;

typedef struct
{
  int x, y, w, h;
} MsqRect;

typedef struct
{
  uint8_t r, g, b, a;
} MsqColor;

/* X-macro theme table. Each entry: NAME, 0xRRGGBBAA. Walk it to define both an
 * enum index and the default palette without repeating the names. */
#define MSQ_THEME_COLORS(_)                    \
  _(MSQ_COL_WINDOW_BG,     0x1B1612FF)          \
  _(MSQ_COL_WIDGET_BG,     0x2E2620FF)          \
  _(MSQ_COL_WIDGET_HOT,    0x3B3129FF)          \
  _(MSQ_COL_WIDGET_ACTIVE, 0x534337FF)          \
  _(MSQ_COL_ACCENT,        0xB07A4CFF)          \
  _(MSQ_COL_ACCENT_DIM,    0x7E5A3BFF)          \
  _(MSQ_COL_TEXT,          0xECE2D8FF)          \
  _(MSQ_COL_TEXT_DIM,      0xA19588FF)          \
  _(MSQ_COL_BORDER,        0x120F0DFF)          \
  _(MSQ_COL_TOOLTIP_BG,    0x171310F0)          \
  _(MSQ_COL_POPUP_BG,      0x241E19FA)          \
  _(MSQ_COL_TITLEBAR,        0x2E2620FF)        \
  _(MSQ_COL_TITLEBAR_ACTIVE, 0x534337FF)        \
  _(MSQ_COL_CLOSE,           0x7E3B33FF)        \
  _(MSQ_COL_CLOSE_HOT,       0xB0463AFF)        \
  _(MSQ_COL_WINDOW_BODY,     0x231D18FF)

typedef enum
{
#define MSQ_X(name, hex) name,
  MSQ_THEME_COLORS (MSQ_X)
#undef MSQ_X
  MSQ_COL_COUNT
} MsqColorSlot;

/* -------------------------------------------------------------------------- */
/*  The single context. Everything lives here; create one, reuse every frame.  */
/* -------------------------------------------------------------------------- */

typedef struct MsqCtx MsqCtx;

/* Lifecycle. */
MsqCtx *msq_create  (SDL_Renderer *renderer, const char *ttf_path, int font_px);
void    msq_destroy (MsqCtx *ctx);

/* Frame boundaries. Feed SDL events between frames, or pump them yourself. */
void msq_begin_frame (MsqCtx *ctx);
void msq_handle_event(MsqCtx *ctx, const SDL_Event *e);
void msq_end_frame   (MsqCtx *ctx);   // draws deferred layers (popups/tooltips)

/* Layout. Open a panel anchored at (x,y); widgets auto-stack downward. */
void msq_panel_begin (MsqCtx *ctx, int x, int y, int w);
void msq_panel_end   (MsqCtx *ctx);
void msq_row_begin   (MsqCtx *ctx);   // lay following widgets left-to-right
void msq_row_end     (MsqCtx *ctx);
void msq_spacer      (MsqCtx *ctx, int px);
void msq_label       (MsqCtx *ctx, const char *text);

/* Identity helpers for disambiguating same-line widgets / loops. */
void msq_push_id     (MsqCtx *ctx, int salt);
void msq_pop_id      (MsqCtx *ctx);

/* Theme. */
void     msq_set_color (MsqCtx *ctx, MsqColorSlot slot, MsqColor c);
MsqColor msq_color     (MsqColorSlot slot);   // default palette lookup

/* -------------------------------------------------------------------------- */
/*  Widgets. Immediate-mode: call every frame, react to the return value.      */
/* -------------------------------------------------------------------------- */

/* Returns true on the frame the button is released inside its bounds. */
bool msq_button (MsqCtx *ctx, const char *label);

/* Toggles *value in place; returns true if it changed this frame. */
bool msq_checkbox (MsqCtx *ctx, const char *label, bool *value);

/* Horizontal slider over [min,max]; edits *value; returns true while dragging. */
bool msq_slider (MsqCtx *ctx, const char *label, float *value,
                 float min, float max);

/* Single-line text input. buf is user-owned, cap includes the NUL.
 * Returns true when the contents changed this frame. */
bool msq_text_input (MsqCtx *ctx, const char *label, char *buf, int cap);

/* Multi-line text editor. buf is user-owned, cap includes the NUL. Supports
 * mouse + Shift selection, arrow / Home / End navigation, and the system
 * clipboard (Ctrl+C copy, Ctrl+X cut, Ctrl+V paste, Ctrl+A select-all).
 * visible_rows sets the height in text rows (<=0 picks a small default) and it
 * scrolls vertically. Returns true on frames where the text changed. */
bool msq_textarea (MsqCtx *ctx, const char *label, char *buf, int cap,
                   int visible_rows);

/* Simple single-selection list capped to visible_rows tall (<=0 means show
 * all). Scrolls with the mouse wheel; a thin 3px bar appears on the right when
 * content overflows. Edits *selected; returns true when the selection changes. */
bool msq_list (MsqCtx *ctx, const char *label, const char *const *items,
               int count, int *selected, int visible_rows);

/* Popup menu. Open it yourself (e.g. on right-click) via msq_popup_open with a
 * stable id, then call msq_popup each frame. Writes the chosen index to
 * *out_choice and returns true on the frame a choice is made. */
MsqId msq_gen_id    (MsqCtx *ctx, const char *seed);
void  msq_popup_open(MsqCtx *ctx, MsqId id, int x, int y);
bool  msq_popup     (MsqCtx *ctx, MsqId id, const char *const *items,
                     int count, int *out_choice);

/* Dropdown = a button that opens a popup of choices anchored beneath it.
 * Edits *selected; returns true when the selection changes. */
bool msq_dropdown (MsqCtx *ctx, const char *label, const char *const *items, int count, int *selected);

/* Tab group: a horizontal strip of tabs flowing left-to-right. The selected tab
 * gets an accent underline. Edits *selected and returns true when it changes;
 * you switch your own content underneath, e.g.
 *
 *   static const char *pages[] = { "Info", "Style", "About" };
 *   msq_tabs (ui, NULL, pages, 3, &page);
 *   if      (page == 0) { ...info widgets...  }
 *   else if (page == 1) { ...style widgets... }
 *   else                { ...about widgets... }                                */
bool msq_tabs (MsqCtx *ctx, const char *label, const char *const *tabs,
               int count, int *selected);

/* Tooltip. Call right after the widget you want it on; it shows after a hover
 * delay when that widget is hot. */
void msq_tooltip (MsqCtx *ctx, const char *text);

/* -------------------------------------------------------------------------- */
/*  Standalone drawing helpers. These take a bare SDL_Renderer (no MsqCtx) so   */
/*  they can be reused for custom widgets or your own scene. Colors are packed  */
/*  0xRRGGBBAA; alpha blends because msq_create enables SDL_BLENDMODE_BLEND.     */
/* -------------------------------------------------------------------------- */

/* Set the renderer draw color from a packed 0xRRGGBBAA value. */
void set_color_hex (SDL_Renderer *r, uint32_t rgba);

/* Soft drop shadow: three translucent black rects stepped down-right from rect. */
void draw_shadow_simple (SDL_Renderer *r, SDL_Rect rect);

/* Beveled border: top/left use color_a, bottom/right use color_b (0xRRGGBBAA). */
void draw_frame (SDL_Renderer *r, int x, int y, int w, int h,
                 int stroke_size, int color_a, int color_b);

/* Chevron arrow inside rect. rotation 0=v 1=< 2=^ 3=> (other values wrap). */
void draw_v_arrow_rotated (SDL_Renderer *r, SDL_Rect rect, int padding,
                           int thickness, int rotation);

/* Filled white triangle with a black outline inside a size x size box at
 * (ox,oy). rotation steps the right angle 90 deg clockwise per unit (wraps):
 * 0 = bottom-right, 1 = bottom-left, 2 = top-left, 3 = top-right. */
void draw_triangle_rotated (SDL_Renderer *r, int ox, int oy, int size, int rotation);


void draw_triangle_angled (SDL_Renderer *r, int ox, int oy, int size, int degrees);

/* -------------------------------------------------------------------------- */
/*  Bitmap "rect" font. A chunky 3x5 pixel font with no blur and no deps. It    */
/*  is always available -- even with SDL2_ttf compiled in -- for the few spots   */
/*  that need a genuinely tiny, crisp, no-blur caption. TTF stays the default    */
/*  for everything else. Glyphs cover ASCII 32..90 (space through 'Z');          */
/*  lowercase is drawn as uppercase.                                            */
/* -------------------------------------------------------------------------- */

typedef enum { MSQ_FONT_TTF, MSQ_FONT_RECT } MsqFontKind;

/* Pick which font the widgets use. MSQ_FONT_RECT works even when SDL2_ttf is
 * compiled in, so you can mix crisp bitmap text with smooth TTF text. */
void msq_set_font       (MsqCtx *ctx, MsqFontKind kind);
void msq_set_rect_scale (MsqCtx *ctx, int scale);   // pixel size of the rect font

/* Standalone rect-font drawing (no MsqCtx), matching the other draw_* helpers. */
void msq_rect_text   (SDL_Renderer *r, int x, int y, const char *s, int scale, MsqColor c);
int  msq_rect_text_w (const char *s, int scale);
int  msq_rect_text_h (int scale);

/* -------------------------------------------------------------------------- */
/*  Child windows. imgui / microui-style draggable, closable, auto-layering     */
/*  panels. State (position, z-order, open flag) is keyed by the title and kept  */
/*  in the context, so you just re-declare them every frame:                     */
/*                                                                              */
/*    if (msq_window_begin (ui, "Inspector", 40, 60, 300, 220, &open)) {         */
/*        ... widgets ...                                                         */
/*        msq_window_end (ui);                                                    */
/*    }                                                                          */
/*                                                                              */
/*  The (x,y,w,h) are only the initial placement the first time a window with    */
/*  that title is seen; afterwards the user drags it by the title bar. Pass a     */
/*  bool* for `open` to get a working close button (the X sets *open=false); pass */
/*  NULL for a window that cannot be closed. The top-most window under the cursor */
/*  receives input, so overlapping windows behave the way you expect.            */
/* -------------------------------------------------------------------------- */

bool msq_window_begin (MsqCtx *ctx, const char *title, int x, int y,
                       int w, int h, bool *open);
void msq_window_end   (MsqCtx *ctx);

#ifdef __cplusplus
}
#endif

/* ========================================================================== */
/*  IMPLEMENTATION                                                            */
/* ========================================================================== */
#ifdef MSQ_IMPLEMENTATION

#include <string.h>
#include <stdlib.h>
#include <math.h>

#define MSQ_TOOLTIP_DELAY_MS 500
#define MSQ_PAD              6
#define MSQ_ROW_H            26
#define MSQ_GAP              4
#define MSQ_TITLEBAR_H       22
#define MSQ_CLOSE_SIZE       14
#define MSQ_CLOSE_PAD        4
#define MSQ_RESIZE_GRIP      16   /* draggable corner box (bottom-right) */
#define MSQ_WIN_MIN_W        90
#define MSQ_WIN_MIN_H        56

typedef struct
{
  MsqId   id;
  int     x, y;
  bool    open;
} MsqPopupState;

/* A popup's body is recorded here when msq_popup runs and replayed in
 * msq_end_frame, so popups paint on top of every panel widget instead of
 * being overdrawn by widgets that are declared after them in the frame. */
typedef struct
{
  MsqRect            box;
  const char *const *items;
  int                count;
} MsqPopupDraw;

/* A single deferred draw op recorded while a child window is being declared.
 * The whole window (decorations + widgets) is recorded, then msq_end_frame
 * replays the windows sorted by z so the focused one paints on top. */
typedef enum
{
  MSQ_CMD_FILL, MSQ_CMD_STROKE, MSQ_CMD_TEXT, MSQ_CMD_RTEXT,
  MSQ_CMD_LINE, MSQ_CMD_CLIP,  MSQ_CMD_TRI,  MSQ_CMD_SHADOW
} MsqCmdKind;

typedef struct
{
  MsqCmdKind  kind;
  int         z;            // window draw rank (sort key)
  int         a, b, c, d;   // geometry ints; meaning depends on kind
  int         e;            // extra int: rect-font scale / clip-enable / tri degrees
  MsqColor    color;
  const char *text;         // points into the per-frame string arena (TEXT/RTEXT)
} MsqCmd;

/* Retained per-window state, keyed by a stable hash of the title. */
typedef struct
{
  MsqId   id;
  MsqRect rect;             // full window rect, title bar included
  bool    open;
  bool    used;             // declared this frame?
  bool    dragging;
  bool    resizing;
  int     drag_off_x, drag_off_y;
  int     z;                // higher = nearer the front
  int     cmd_start, cmd_count;   // this frame's slice of the command buffer
} MsqWindow;

struct MsqCtx
{
  SDL_Renderer *ren;
#ifndef MSQ_NO_TTF
  TTF_Font *font;
#endif
  int font_px;

  /* palette (copy of defaults, user-overridable) */
  MsqColor pal[MSQ_COL_COUNT];

  /* input snapshot for the current frame */
  int   mouse_x, mouse_y;
  bool  mouse_down, mouse_pressed, mouse_released;
  bool  mouse_down_r, mouse_pressed_r;
  int   wheel;
  char  text_in[32];      // utf8 text events this frame
  int   text_in_len;
  bool  key_back, key_left, key_right, key_enter, key_tab;
  bool  key_up, key_down, key_home, key_end, key_del;
  bool  key_copy, key_cut, key_paste, key_selectall;   // Ctrl+C/X/V/A

  /* interaction identity */
  MsqId hot, active, focus;
  MsqId id_stack[MSQ_ID_STACK];
  int   id_top;

  /* layout cursor */
  int  panel_x, panel_y, panel_w;
  int  cur_x, cur_y;
  int  row_active, row_start_y, row_max_h;
  int  line;              // synthetic source-line counter for auto-ids

  /* one popup at a time is plenty for a tiny lib, but keep a small ring */
  MsqPopupState popups[8];
  int           popup_count;

  /* per-list vertical scroll offsets, keyed by list id */
  struct { MsqId id; int off; } scroll[8];
  int scroll_count;

  /* deferred popup bodies, flushed on top of everything in msq_end_frame */
  MsqPopupDraw  popup_draws[8];
  int           popup_draw_count;

  /* tooltip */
  MsqId   tip_hot;
  Uint32  tip_since;
  char    tip_text[256];
  bool    tip_pending;
  int     tip_x, tip_y;

  /* text-input editing scratch */
  MsqId caret_owner;
  int   caret;            // index into the active buffer
  int   sel_anchor;       // selection anchor for the focused text editor

  /* runtime font selection: TTF (if built) or the bundled 3x5 rect font */
  int  font_kind;         // MsqFontKind
  int  rect_scale;

  /* child windows + deferred draw command buffer */
  MsqWindow  windows[MSQ_MAX_WINDOWS];
  int        win_count;
  int        z_counter;
  MsqId      front_win;   // top-most window under the cursor this frame
  MsqWindow *cur_win;     // window currently between begin/end (no nesting)
  int        win_saved_mx, win_saved_my;
  bool       win_grip_hot;        // is the current window's resize grip hovered?
  bool       clip_active;         // a window body clip is in force
  MsqRect    clip_base;           // ...this rect; msq__clip(NULL) restores it

  bool    recording;      // are the draw helpers recording into cmds[] ?
  int     rec_z;          // z stamped on recorded commands
  MsqCmd  cmds[MSQ_MAX_DRAW_CMDS];
  int     cmd_count;
  char    arena[MSQ_DRAW_STR_ARENA];
  int     arena_used;
};

/* ---- color helpers ------------------------------------------------------- */

static const MsqColor msq__defaults[MSQ_COL_COUNT] = {
#define MSQ_X(name, hex)                                  \
  { (uint8_t) ((hex) >> 24), (uint8_t) ((hex) >> 16),     \
    (uint8_t) ((hex) >> 8),  (uint8_t) (hex) },
  MSQ_THEME_COLORS (MSQ_X)
#undef MSQ_X
};

MsqColor msq_color (MsqColorSlot slot)
{
  if (slot < 0 || slot >= MSQ_COL_COUNT)
    {
      MsqColor z = { 255, 0, 255, 255 };
      return z;
    }
  return msq__defaults[slot];
}

void msq_set_color (MsqCtx *ctx, MsqColorSlot slot, MsqColor c)
{
  if (slot >= 0 && slot < MSQ_COL_COUNT)
    ctx->pal[slot] = c;
}

/* Forward decls for the deferred draw command buffer (defined just below). */
static void        msq__push_cmd  (MsqCtx *ctx, MsqCmd c);
static const char *msq__arena_dup (MsqCtx *ctx, const char *s);

/* Low-level primitives. When a window is being declared (ctx->recording) these
 * append a command instead of drawing, so the window can be replayed in
 * z-order; otherwise they draw immediately, exactly as before. */
static void msq__fill_c (MsqCtx *ctx, MsqRect r, MsqColor col)
{
  if (ctx->recording)
    {
      MsqCmd c = { 0 };
      c.kind = MSQ_CMD_FILL;
      c.a = r.x; c.b = r.y; c.c = r.w; c.d = r.h; c.color = col;
      msq__push_cmd (ctx, c);
      return;
    }
  SDL_Rect rr = { r.x, r.y, r.w, r.h };
  SDL_SetRenderDrawColor (ctx->ren, col.r, col.g, col.b, col.a);
  SDL_RenderFillRect (ctx->ren, &rr);
}

static void msq__stroke_c (MsqCtx *ctx, MsqRect r, MsqColor col)
{
  if (ctx->recording)
    {
      MsqCmd c = { 0 };
      c.kind = MSQ_CMD_STROKE;
      c.a = r.x; c.b = r.y; c.c = r.w; c.d = r.h; c.color = col;
      msq__push_cmd (ctx, c);
      return;
    }
  SDL_Rect rr = { r.x, r.y, r.w, r.h };
  SDL_SetRenderDrawColor (ctx->ren, col.r, col.g, col.b, col.a);
  SDL_RenderDrawRect (ctx->ren, &rr);
}

static void msq__fill (MsqCtx *ctx, MsqRect r, MsqColorSlot s)
{
  msq__fill_c (ctx, r, ctx->pal[s]);
}

static void msq__stroke (MsqCtx *ctx, MsqRect r, MsqColorSlot s)
{
  msq__stroke_c (ctx, r, ctx->pal[s]);
}

static void msq__line (MsqCtx *ctx, int x0, int y0, int x1, int y1, MsqColor col)
{
  if (ctx->recording)
    {
      MsqCmd c = { 0 };
      c.kind = MSQ_CMD_LINE;
      c.a = x0; c.b = y0; c.c = x1; c.d = y1; c.color = col;
      msq__push_cmd (ctx, c);
      return;
    }
  SDL_SetRenderDrawColor (ctx->ren, col.r, col.g, col.b, col.a);
  SDL_RenderDrawLine (ctx->ren, x0, y0, x1, y1);
}

/* Set the clip rect; pass NULL to clear it. When a window body clip is active
 * (ctx->clip_active) a passed rect is intersected with it, and NULL restores the
 * body clip rather than clearing -- so a list/textarea's own clip never lets
 * content escape the window, and clearing it falls back to the window body. */
static void msq__clip (MsqCtx *ctx, const MsqRect *r)
{
  MsqRect eff;
  bool enable = false;
  if (r)
    {
      eff = *r;
      if (ctx->clip_active)
        {
          MsqRect base = ctx->clip_base;
          int x0 = eff.x > base.x ? eff.x : base.x;
          int y0 = eff.y > base.y ? eff.y : base.y;
          int x1 = eff.x + eff.w < base.x + base.w ? eff.x + eff.w : base.x + base.w;
          int y1 = eff.y + eff.h < base.y + base.h ? eff.y + eff.h : base.y + base.h;
          eff.x = x0; eff.y = y0;
          eff.w = x1 > x0 ? x1 - x0 : 0;
          eff.h = y1 > y0 ? y1 - y0 : 0;
        }
      enable = true;
    }
  else if (ctx->clip_active)
    {
      eff = ctx->clip_base;
      enable = true;
    }

  if (ctx->recording)
    {
      MsqCmd c = { 0 };
      c.kind = MSQ_CMD_CLIP;
      if (enable) { c.a = eff.x; c.b = eff.y; c.c = eff.w; c.d = eff.h; c.e = 1; }
      msq__push_cmd (ctx, c);
      return;
    }
  if (enable)
    {
      SDL_Rect cl = { eff.x, eff.y, eff.w, eff.h };
      SDL_RenderSetClipRect (ctx->ren, &cl);
    }
  else
    SDL_RenderSetClipRect (ctx->ren, NULL);
}

static void msq__shadow_rec (MsqCtx *ctx, MsqRect r)
{
  if (ctx->recording)
    {
      MsqCmd c = { 0 };
      c.kind = MSQ_CMD_SHADOW;
      c.a = r.x; c.b = r.y; c.c = r.w; c.d = r.h;
      msq__push_cmd (ctx, c);
      return;
    }
  SDL_Rect rr = { r.x, r.y, r.w, r.h };
  draw_shadow_simple (ctx->ren, rr);
}

static void msq__tri_angled (MsqCtx *ctx, int ox, int oy, int size, int deg)
{
  if (ctx->recording)
    {
      MsqCmd c = { 0 };
      c.kind = MSQ_CMD_TRI;
      c.a = ox; c.b = oy; c.c = size; c.e = deg;
      msq__push_cmd (ctx, c);
      return;
    }
  draw_triangle_angled (ctx->ren, ox, oy, size, deg);
}

static void msq__push_cmd (MsqCtx *ctx, MsqCmd c)
{
  if (ctx->cmd_count >= MSQ_MAX_DRAW_CMDS)
    return;
  c.z = ctx->rec_z;
  ctx->cmds[ctx->cmd_count++] = c;
}

static const char *msq__arena_dup (MsqCtx *ctx, const char *s)
{
  if (!s) s = "";
  int n = (int) strlen (s) + 1;
  if (ctx->arena_used + n > MSQ_DRAW_STR_ARENA)
    return NULL;
  char *d = ctx->arena + ctx->arena_used;
  memcpy (d, s, (size_t) n);
  ctx->arena_used += n;
  return d;
}

static bool msq__hit (MsqRect r, int x, int y)
{
  return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

/* ---- standalone drawing helpers ------------------------------------------ */

void set_color_hex (SDL_Renderer *r, uint32_t rgba)
{
  SDL_SetRenderDrawColor (r, (rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF,
                          (rgba >> 8) & 0xFF, rgba & 0xFF);
}

void draw_shadow_simple (SDL_Renderer *r, SDL_Rect rect)
{
  set_color_hex (r, 0x00000033);
  rect.x = rect.x + 2;
  rect.y = rect.y + 2;
  SDL_RenderFillRect (r, &rect);

  set_color_hex (r, 0x00000022);
  rect.x = rect.x + 2;
  rect.y = rect.y + 2;
  SDL_RenderFillRect (r, &rect);

  set_color_hex (r, 0x00000011);
  rect.x = rect.x + 2;
  rect.y = rect.y + 2;
  SDL_RenderFillRect (r, &rect);
}

void draw_frame (SDL_Renderer *r, int x, int y, int w, int h, int stroke_size,
                 int color_a, int color_b)
{
  SDL_Rect top = { x, y, w, stroke_size };
  set_color_hex (r, color_a);
  SDL_RenderFillRect (r, &top);

  SDL_Rect left = { x, y + stroke_size, stroke_size, h - stroke_size };
  set_color_hex (r, color_a);
  SDL_RenderFillRect (r, &left);

  SDL_Rect bottom = { x, y + h - stroke_size, w, stroke_size };
  set_color_hex (r, color_b);
  SDL_RenderFillRect (r, &bottom);

  SDL_Rect right = { x + w - stroke_size, y, stroke_size, h };
  set_color_hex (r, color_b);
  SDL_RenderFillRect (r, &right);
}

void draw_v_arrow_rotated (SDL_Renderer *r, SDL_Rect rect, int padding,
                           int thickness, int rotation)
{
  SDL_SetRenderDrawColor (r, 0, 0, 0, 255);
  int avail_w = rect.w - (padding * 2);
  int avail_h = rect.h - (padding * 2);
  if (avail_w <= 0 || avail_h <= 0) return;
  int size = (avail_w < avail_h) ? avail_w : avail_h;
  int ox = rect.x + padding + (avail_w - size) / 2;
  int oy = rect.y + padding + (avail_h - size) / 2;

  rotation = ((rotation % 4) + 4) % 4;

  int half = size / 2;

  for (int t = 0; t < thickness; ++t) {
    /* Inner offset clamped so the two arms don't cross the centerline.
     * This is what keeps the V notch instead of filling into a solid triangle. */
    int ti = (t > half) ? half : t;

    int x1a, y1a, x2a, y2a;  /* left arm */
    int x1b, y1b, x2b, y2b;  /* right arm */

    switch (rotation) {
      case 0:  /* V (down) */
        x1a = ox + t;        y1a = oy;        x2a = ox + half + ti; y2a = oy + size;
        x1b = ox + size - t; y1b = oy;        x2b = ox + half - ti; y2b = oy + size;
        break;
      case 1:  /* < (left) */
        x1a = ox + size; y1a = oy + t;        x2a = ox; y2a = oy + half + ti;
        x1b = ox + size; y1b = oy + size - t; x2b = ox; y2b = oy + half - ti;
        break;
      case 2:  /* ^ (up) */
        x1a = ox + t;        y1a = oy + size; x2a = ox + half + ti; y2a = oy;
        x1b = ox + size - t; y1b = oy + size; x2b = ox + half - ti; y2b = oy;
        break;
      default: /* 3: > (right) */
        x1a = ox; y1a = oy + t;        x2a = ox + size; y2a = oy + half + ti;
        x1b = ox; y1b = oy + size - t; x2b = ox + size; y2b = oy + half - ti;
        break;
    }
    SDL_RenderDrawLine (r, x1a, y1a, x2a, y2a);
    SDL_RenderDrawLine (r, x1b, y1b, x2b, y2b);
  }
}

/* Flat-fill an arbitrary triangle by scanline: for each row, span between the
 * leftmost and rightmost edge crossings. */
static void msq__fill_tri (SDL_Renderer *ren, int x0, int y0, int x1, int y1, int x2, int y2)
{
  int xs[3] = { x0, x1, x2 };
  int ys[3] = { y0, y1, y2 };
  int ymin = ys[0], ymax = ys[0];
  for (int i = 1; i < 3; i++)
    {
      if (ys[i] < ymin) ymin = ys[i];
      if (ys[i] > ymax) ymax = ys[i];
    }
  for (int y = ymin; y <= ymax; y++)
    {
      int xl = 0, xr = 0;
      bool have = false;
      for (int e = 0; e < 3; e++)
        {
          int ax = xs[e],           ay = ys[e];
          int bx = xs[(e + 1) % 3], by = ys[(e + 1) % 3];
          int lo = ay < by ? ay : by;
          int hi = ay > by ? ay : by;
          if (ay == by || y < lo || y > hi) continue;
          int x = ax + (int) ((long) (bx - ax) * (y - ay) / (by - ay));
          if (!have) { xl = xr = x; have = true; }
          else { if (x < xl) xl = x; if (x > xr) xr = x; }
        }
      if (have)
        SDL_RenderDrawLine (ren, xl, y, xr, y);
    }
}


void draw_triangle_angled (SDL_Renderer *r, int ox, int oy, int size, int degrees)
{
  // 1. Normalise the angle into a positive 0-359 range
  degrees = ((degrees % 360) + 360) % 360;
  // 2. Convert degrees to a 0-3 step index (closest 90-degree quadrant)
  // Adding 45 rounds it to the nearest quadrant rather than just flooring it.
  int rotation = ((degrees + 45) / 90) % 4;
  // 3. Base triangle that points UP at rotation 0:
  // tip at top-center, two corners along the bottom edge.
  int u[3] = { size / 2, 0,    size };
  int v[3] = { 0,        size, size };
  // 4. Each clockwise 90-deg spin advances the tip Up->Right->Down->Left,
  // which matches the desired mapping directly, so spin_steps == rotation.
  int spin_steps = rotation;
  // Spin each vertex 90 deg clockwise about the box center
  for (int i = 0; i < 3; i++)
    for (int k = 0; k < spin_steps; k++)
      {
        int nu = size - v[i];
        int nv = u[i];
        u[i] = nu; v[i] = nv;
      }
  int x0 = ox + u[0], y0 = oy + v[0];
  int x1 = ox + u[1], y1 = oy + v[1];
  int x2 = ox + u[2], y2 = oy + v[2];
  SDL_SetRenderDrawColor (r, 255, 255, 255, 255);
  msq__fill_tri (r, x0, y0, x1, y1, x2, y2);
  //SDL_SetRenderDrawColor (r, 0, 0, 0, 255);
  //SDL_RenderDrawLine (r, x0, y0, x1, y1);
  //SDL_RenderDrawLine (r, x1, y1, x2, y2);
  //SDL_RenderDrawLine (r, x2, y2, x0, y0);
}



void draw_triangle_rotated (SDL_Renderer *r, int ox, int oy, int size, int rotation)
{
  rotation = ((rotation % 4) + 4) % 4;

  /* base right-triangle (rotation 0): right angle at the bottom-right corner,
   * hypotenuse from top-right down to bottom-left. */
  int u[3] = { size, size, 0 };
  int v[3] = { 0,    size, size };

  /* spin each vertex 90 deg clockwise about the box center, rotation times:
   * (u,v) -> (size - v, u). */
  for (int i = 0; i < 3; i++)
    for (int k = 0; k < rotation; k++)
      {
        int nu = size - v[i];
        int nv = u[i];
        u[i] = nu; v[i] = nv;
      }

  int x0 = ox + u[0], y0 = oy + v[0];
  int x1 = ox + u[1], y1 = oy + v[1];
  int x2 = ox + u[2], y2 = oy + v[2];

  SDL_SetRenderDrawColor (r, 255, 255, 255, 255);
  msq__fill_tri (r, x0, y0, x1, y1, x2, y2);

  SDL_SetRenderDrawColor (r, 0, 0, 0, 255);
  SDL_RenderDrawLine (r, x0, y0, x1, y1);
  SDL_RenderDrawLine (r, x1, y1, x2, y2);
  SDL_RenderDrawLine (r, x2, y2, x0, y0);
}

/* ---- hashing for stable widget ids --------------------------------------- */

/* FNV-1a over a string, mixed with the id stack so identical strings under a
 * different msq_push_id salt produce different ids. */
static MsqId msq__hash_str (MsqCtx *ctx, const char *s)
{
  MsqId h = 2166136261u;
  for (int i = 0; i < ctx->id_top; i++)
    {
      h ^= (MsqId) ctx->id_stack[i];
      h *= 16777619u;
    }
  if (s)
    for (const unsigned char *p = (const unsigned char *) s; *p; p++)
      {
        h ^= *p;
        h *= 16777619u;
      }
  /* fold in a per-frame line counter so two buttons with the same label still
   * differ by call order, the immediate-mode trick. */
  h ^= (MsqId) (++ctx->line);
  h *= 16777619u;
  return h ? h : 1u;
}

MsqId msq_gen_id (MsqCtx *ctx, const char *seed)
{
  /* stable across frames: no line mixing */
  (void) ctx;
  MsqId h = 2166136261u;
  if (seed)
    for (const unsigned char *p = (const unsigned char *) seed; *p; p++)
      {
        h ^= *p;
        h *= 16777619u;
      }
  return h ? h : 1u;
}

void msq_push_id (MsqCtx *ctx, int salt)
{
  if (ctx->id_top < MSQ_ID_STACK)
    ctx->id_stack[ctx->id_top++] = (MsqId) salt;
}

void msq_pop_id (MsqCtx *ctx)
{
  if (ctx->id_top > 0)
    ctx->id_top--;
}

/* ---- bundled 3x5 "rect" bitmap font (always available) ------------------- */

/* Each glyph is 5 rows; the top 3 bits of every byte are the columns. Covers
 * ASCII 32..90 (space..'Z'). Lowercase is upper-cased; anything else is skipped.
 * This is the same crisp, no-blur font you want for tiny title-bar labels. */
static const uint8_t MSQ_RFONT[][5] = {
  {0x00,0x00,0x00,0x00,0x00}, /* ' ' */
  {0x40,0x40,0x40,0x00,0x40}, /* '!' */
  {0xA0,0xA0,0x00,0x00,0x00}, /* '"' */
  {0xA0,0xE0,0xA0,0xE0,0xA0}, /* '#' */
  {0x60,0xC0,0x60,0xC0,0x60}, /* '$' */
  {0x80,0x20,0x40,0x80,0x20}, /* '%' */
  {0x40,0xA0,0x40,0xA0,0xC0}, /* '&' */
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
  {0x60,0xA0,0xE0,0x80,0x60}, /* '@' */
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

static void msq__rglyph (SDL_Renderer *r, int cx, int cy, char ch, int scale)
{
  int idx = (int) ch - 32;
  if (idx < 0 || idx >= (int) (sizeof MSQ_RFONT / sizeof MSQ_RFONT[0]))
    return;
  const uint8_t *g = MSQ_RFONT[idx];
  for (int row = 0; row < 5; row++)
    for (int col = 0; col < 3; col++)
      if (g[row] & (0x80 >> col))
        {
          SDL_Rect px = { cx + col * scale, cy + row * scale, scale, scale };
          SDL_RenderFillRect (r, &px);
        }
}

static void msq__rtext_draw (SDL_Renderer *r, int x, int y, const char *s,
                             int scale, MsqColor c)
{
  if (!s) return;
  if (scale < 1) scale = 1;
  SDL_SetRenderDrawColor (r, c.r, c.g, c.b, c.a);
  for (int i = 0; s[i]; i++)
    {
      char ch = s[i];
      if (ch >= 'a' && ch <= 'z') ch = (char) (ch - 32);
      msq__rglyph (r, x + i * (4 * scale), y, ch, scale);
    }
}

/* public standalone wrappers (mirror set_color_hex / draw_frame and friends) */
void msq_rect_text (SDL_Renderer *r, int x, int y, const char *s, int scale, MsqColor c)
{
  msq__rtext_draw (r, x, y, s, scale, c);
}
int msq_rect_text_w (const char *s, int scale)
{
  return s ? (int) strlen (s) * 4 * (scale > 0 ? scale : 1) : 0;
}
int msq_rect_text_h (int scale)
{
  return 5 * (scale > 0 ? scale : 1);
}

/* ---- widget text: TTF when available + selected, else the rect font ------ */

static bool msq__use_rect (MsqCtx *ctx)
{
#ifdef MSQ_NO_TTF
  (void) ctx;
  return true;
#else
  return ctx->font_kind == MSQ_FONT_RECT || !ctx->font;
#endif
}

static void msq__text_draw (MsqCtx *ctx, int x, int y, const char *s,
                            MsqColor c, bool rect, int scale)
{
  if (rect)
    {
      msq__rtext_draw (ctx->ren, x, y, s, scale, c);
      return;
    }
#ifndef MSQ_NO_TTF
  if (!s || !*s || !ctx->font)
    return;
  SDL_Color sc = { c.r, c.g, c.b, c.a };
  SDL_Surface *surf = TTF_RenderUTF8_Blended (ctx->font, s, sc);
  if (!surf)
    return;
  SDL_Texture *tex = SDL_CreateTextureFromSurface (ctx->ren, surf);
  SDL_Rect dst = { x, y, surf->w, surf->h };
  SDL_RenderCopy (ctx->ren, tex, NULL, &dst);
  SDL_DestroyTexture (tex);
  SDL_FreeSurface (surf);
#else
  msq__rtext_draw (ctx->ren, x, y, s, scale, c);
#endif
}

static void msq__text (MsqCtx *ctx, int x, int y, const char *s, MsqColorSlot col)
{
  MsqColor c = ctx->pal[col];
  bool rect = msq__use_rect (ctx);
  int scale = ctx->rect_scale;
  if (ctx->recording)
    {
      MsqCmd cmd = { 0 };
      cmd.kind = rect ? MSQ_CMD_RTEXT : MSQ_CMD_TEXT;
      cmd.a = x; cmd.b = y; cmd.e = scale; cmd.color = c;
      cmd.text = msq__arena_dup (ctx, s);
      if (cmd.text) msq__push_cmd (ctx, cmd);
      return;
    }
  msq__text_draw (ctx, x, y, s, c, rect, scale);
}

static int msq__text_w (MsqCtx *ctx, const char *s)
{
  if (!s) return 0;
  if (msq__use_rect (ctx))
    return (int) strlen (s) * 4 * ctx->rect_scale;
#ifndef MSQ_NO_TTF
  {
    int w = 0, h = 0;
    if (ctx->font) TTF_SizeUTF8 (ctx->font, s, &w, &h);
    return w;
  }
#else
  return (int) strlen (s) * 4 * ctx->rect_scale;
#endif
}

static int msq__text_h (MsqCtx *ctx)
{
  if (msq__use_rect (ctx))
    return 5 * ctx->rect_scale;
#ifndef MSQ_NO_TTF
  return ctx->font ? TTF_FontHeight (ctx->font) : 5 * ctx->rect_scale;
#else
  return 5 * ctx->rect_scale;
#endif
}

/* ---- lifecycle ----------------------------------------------------------- */

MsqCtx *msq_create (SDL_Renderer *renderer, const char *ttf_path, int font_px)
{
  MsqCtx *ctx = (MsqCtx *) calloc (1, sizeof *ctx);
  if (!ctx)
    return NULL;
  ctx->ren = renderer;
  ctx->font_px = font_px > 0 ? font_px : 16;
  ctx->rect_scale = ctx->font_px >= 18 ? 3 : 2;
  memcpy (ctx->pal, msq__defaults, sizeof ctx->pal);
#ifndef MSQ_NO_TTF
  ctx->font_kind = MSQ_FONT_TTF;
  if (!TTF_WasInit ())
    TTF_Init ();
  if (ttf_path)
    ctx->font = TTF_OpenFont (ttf_path, ctx->font_px);
#else
  ctx->font_kind = MSQ_FONT_RECT;
  (void) ttf_path;
#endif
  SDL_SetRenderDrawBlendMode (renderer, SDL_BLENDMODE_BLEND);
  return ctx;
}

void msq_set_font (MsqCtx *ctx, MsqFontKind kind)
{
  if (ctx) ctx->font_kind = (int) kind;
}

void msq_set_rect_scale (MsqCtx *ctx, int scale)
{
  if (ctx && scale > 0) ctx->rect_scale = scale;
}

void msq_destroy (MsqCtx *ctx)
{
  if (!ctx)
    return;
#ifndef MSQ_NO_TTF
  if (ctx->font)
    TTF_CloseFont (ctx->font);
#endif
  free (ctx);
}

/* ---- event intake -------------------------------------------------------- */

void msq_handle_event (MsqCtx *ctx, const SDL_Event *e)
{
  switch (e->type)
    {
    case SDL_MOUSEMOTION:
      ctx->mouse_x = e->motion.x;
      ctx->mouse_y = e->motion.y;
      break;
    case SDL_MOUSEBUTTONDOWN:
      if (e->button.button == SDL_BUTTON_LEFT)
        {
          ctx->mouse_down = true;
          ctx->mouse_pressed = true;
        }
      else if (e->button.button == SDL_BUTTON_RIGHT)
        {
          ctx->mouse_down_r = true;
          ctx->mouse_pressed_r = true;
        }
      break;
    case SDL_MOUSEBUTTONUP:
      if (e->button.button == SDL_BUTTON_LEFT)
        {
          ctx->mouse_down = false;
          ctx->mouse_released = true;
        }
      else if (e->button.button == SDL_BUTTON_RIGHT)
        ctx->mouse_down_r = false;
      break;
    case SDL_MOUSEWHEEL:
      ctx->wheel += e->wheel.y;
      break;
    case SDL_TEXTINPUT:
      {
        int n = (int) strlen (e->text.text);
        if (ctx->text_in_len + n < (int) sizeof ctx->text_in)
          {
            memcpy (ctx->text_in + ctx->text_in_len, e->text.text, n);
            ctx->text_in_len += n;
            ctx->text_in[ctx->text_in_len] = 0;
          }
      }
      break;
    case SDL_KEYDOWN:
      {
        bool ctrl = (e->key.keysym.mod & KMOD_CTRL) != 0;
        switch (e->key.keysym.sym)
          {
          case SDLK_BACKSPACE: ctx->key_back = true; break;
          case SDLK_DELETE:    ctx->key_del = true; break;
          case SDLK_LEFT:      ctx->key_left = true; break;
          case SDLK_RIGHT:     ctx->key_right = true; break;
          case SDLK_UP:        ctx->key_up = true; break;
          case SDLK_DOWN:      ctx->key_down = true; break;
          case SDLK_HOME:      ctx->key_home = true; break;
          case SDLK_END:       ctx->key_end = true; break;
          case SDLK_RETURN:
          case SDLK_KP_ENTER:  ctx->key_enter = true; break;
          case SDLK_TAB:       ctx->key_tab = true; break;
          case SDLK_c:         if (ctrl) ctx->key_copy = true; break;
          case SDLK_x:         if (ctrl) ctx->key_cut = true; break;
          case SDLK_v:         if (ctrl) ctx->key_paste = true; break;
          case SDLK_a:         if (ctrl) ctx->key_selectall = true; break;
          default: break;
          }
      }
      break;
    default:
      break;
    }
}

/* ---- frame --------------------------------------------------------------- */

void msq_begin_frame (MsqCtx *ctx)
{
  ctx->hot = 0;
  ctx->line = 0;
  ctx->id_top = 0;
  ctx->tip_pending = false;
  ctx->popup_draw_count = 0;

  /* reset the window command buffer for the new frame */
  ctx->cmd_count = 0;
  ctx->arena_used = 0;
  ctx->recording = false;
  ctx->clip_active = false;
  ctx->cur_win = NULL;
  for (int i = 0; i < ctx->win_count; i++)
    ctx->windows[i].used = false;

  /* the top-most open window under the cursor receives input this frame. This
   * uses last frame's geometry, which only lags a single frame -- fine. */
  ctx->front_win = 0;
  {
    bool found = false;
    int  best = 0;
    for (int i = 0; i < ctx->win_count; i++)
      {
        MsqWindow *w = &ctx->windows[i];
        if (!w->id || !w->open)
          continue;
        if (msq__hit (w->rect, ctx->mouse_x, ctx->mouse_y)
            && (!found || w->z > best))
          {
            found = true;
            best = w->z;
            ctx->front_win = w->id;
          }
      }
  }
  /* per-frame edge flags reset; level flags (mouse_down) persist */
  /* note: pressed/released cleared in end_frame after widgets read them */
}

static void msq__advance (MsqCtx *ctx, int w, int h)
{
  if (ctx->row_active)
    {
      ctx->cur_x += w + MSQ_GAP;
      if (h > ctx->row_max_h)
        ctx->row_max_h = h;
    }
  else
    {
      ctx->cur_y += h + MSQ_GAP;
    }
}

/* Reserve the next widget rectangle from the layout cursor. */
static MsqRect msq__next (MsqCtx *ctx, int w, int h)
{
  MsqRect r;
  r.x = ctx->cur_x;
  r.y = ctx->cur_y;
  r.w = w;
  r.h = h;
  if (!ctx->row_active && w <= 0)
    r.w = ctx->panel_w - (ctx->cur_x - ctx->panel_x) - MSQ_PAD;
  return r;
}

void msq_panel_begin (MsqCtx *ctx, int x, int y, int w)
{
  ctx->panel_x = x;
  ctx->panel_y = y;
  ctx->panel_w = w;
  ctx->cur_x = x + MSQ_PAD;
  ctx->cur_y = y + MSQ_PAD;
  ctx->row_active = 0;
}

void msq_panel_end (MsqCtx *ctx)
{
  /* draw a subtle panel backing behind everything we just emitted by
   * painting border lines; the fill is expected from the app's clear. */
  MsqRect r = { ctx->panel_x, ctx->panel_y, ctx->panel_w,
                ctx->cur_y - ctx->panel_y + MSQ_PAD };
  msq__stroke (ctx, r, MSQ_COL_BORDER);
}

void msq_row_begin (MsqCtx *ctx)
{
  ctx->row_active = 1;
  ctx->row_start_y = ctx->cur_y;
  ctx->row_max_h = 0;
}

void msq_row_end (MsqCtx *ctx)
{
  ctx->row_active = 0;
  ctx->cur_x = ctx->panel_x + MSQ_PAD;
  ctx->cur_y = ctx->row_start_y + ctx->row_max_h + MSQ_GAP;
}

void msq_spacer (MsqCtx *ctx, int px)
{
  if (ctx->row_active)
    ctx->cur_x += px;
  else
    ctx->cur_y += px;
}

void msq_label (MsqCtx *ctx, const char *text)
{
  MsqRect r = msq__next (ctx, msq__text_w (ctx, text) + 2, MSQ_ROW_H);
  int th = msq__text_h (ctx);
  msq__text (ctx, r.x, r.y + (r.h - th) / 2, text, MSQ_COL_TEXT);
  msq__advance (ctx, r.w, r.h);
}

/* shared button-style interaction; returns release-click */
static bool msq__behave (MsqCtx *ctx, MsqId id, MsqRect r, bool *out_hot)
{
  bool hovered = msq__hit (r, ctx->mouse_x, ctx->mouse_y);
  if (hovered)
    ctx->hot = id;
  if (out_hot)
    *out_hot = hovered;
  bool clicked = false;
  if (ctx->active == id)
    {
      if (ctx->mouse_released)
        {
          if (hovered)
            clicked = true;
          ctx->active = 0;
        }
    }
  else if (hovered && ctx->mouse_pressed && ctx->active == 0)
    {
      ctx->active = id;
    }
  return clicked;
}

/* ---- button -------------------------------------------------------------- */

bool msq_button (MsqCtx *ctx, const char *label)
{
  MsqId id = msq__hash_str (ctx, label);
  int w = msq__text_w (ctx, label) + 2 * MSQ_PAD * 2;
  MsqRect r = msq__next (ctx, ctx->row_active ? w : -1, MSQ_ROW_H);
  if (!ctx->row_active)
    r.w = w; /* buttons size to content even in column mode */

  bool hot;
  bool clicked = msq__behave (ctx, id, r, &hot);

  MsqColorSlot bg = MSQ_COL_WIDGET_BG;
  if (ctx->active == id)
    bg = MSQ_COL_WIDGET_ACTIVE;
  else if (hot)
    bg = MSQ_COL_WIDGET_HOT;

  msq__fill (ctx, r, bg);
  msq__stroke (ctx, r, MSQ_COL_BORDER);
  int tw = msq__text_w (ctx, label), th = msq__text_h (ctx);
  msq__text (ctx, r.x + (r.w - tw) / 2, r.y + (r.h - th) / 2, label,
             MSQ_COL_TEXT);

  ctx->tip_hot = hot ? id : (ctx->tip_hot == id ? 0 : ctx->tip_hot);
  msq__advance (ctx, r.w, r.h);
  return clicked;
}

/* ---- checkbox ------------------------------------------------------------ */

bool msq_checkbox (MsqCtx *ctx, const char *label, bool *value)
{
  MsqId id = msq__hash_str (ctx, label);
  int box = MSQ_ROW_H - 8;
  int tw = msq__text_w (ctx, label);
  MsqRect r = msq__next (ctx, box + 6 + tw, MSQ_ROW_H);

  bool hot;
  bool clicked = msq__behave (ctx, id, r, &hot);
  bool changed = false;
  if (clicked)
    {
      *value = !*value;
      changed = true;
    }

  MsqRect b = { r.x, r.y + (r.h - box) / 2, box, box };
  msq__fill (ctx, b, hot ? MSQ_COL_WIDGET_HOT : MSQ_COL_WIDGET_BG);
  msq__stroke (ctx, b, MSQ_COL_BORDER);
  if (*value)
    {
      MsqRect tick = { b.x + 4, b.y + 4, box - 8, box - 8 };
      msq__fill (ctx, tick, MSQ_COL_ACCENT);
    }
  int th = msq__text_h (ctx);
  msq__text (ctx, b.x + box + 6, r.y + (r.h - th) / 2, label, MSQ_COL_TEXT);

  ctx->tip_hot = hot ? id : ctx->tip_hot;
  msq__advance (ctx, r.w, r.h);
  return changed;
}

/* ---- slider -------------------------------------------------------------- */

bool msq_slider (MsqCtx *ctx, const char *label, float *value, float min, float max)
{
  MsqId id = msq__hash_str (ctx, label);
  MsqRect r = msq__next (ctx, ctx->row_active ? 160 : -1, MSQ_ROW_H);
  if (!ctx->row_active)
    r.w = ctx->panel_w - (r.x - ctx->panel_x) - MSQ_PAD;

  bool hot = msq__hit (r, ctx->mouse_x, ctx->mouse_y);
  if (hot)
    ctx->hot = id;
  if (hot && ctx->mouse_pressed && ctx->active == 0)
    ctx->active = id;
  if (ctx->active == id && ctx->mouse_released)
    ctx->active = 0;

  bool changed = false;
  if (ctx->active == id)
    {
      float t = (float) (ctx->mouse_x - r.x) / (float) (r.w);
      if (t < 0) t = 0;
      if (t > 1) t = 1;
      float nv = min + t * (max - min);
      if (nv != *value)
        {
          *value = nv;
          changed = true;
        }
    }

  float t = (max > min) ? (*value - min) / (max - min) : 0.f;
  if (t < 0) t = 0;
  if (t > 1) t = 1;

  msq__fill (ctx, r, MSQ_COL_WIDGET_BG);
  MsqRect fillr = { r.x, r.y, (int) (r.w * t), r.h };
  msq__fill (ctx, fillr, MSQ_COL_ACCENT_DIM);
  int kn = 8;
  MsqRect knob = { r.x + (int) (t * (r.w - kn)), r.y, kn, r.h };
  msq__fill (ctx, knob, (ctx->active == id || hot) ? MSQ_COL_ACCENT
                                                   : MSQ_COL_WIDGET_ACTIVE);
  msq__stroke (ctx, r, MSQ_COL_BORDER);

  char buf[96];
  SDL_snprintf (buf, sizeof buf, "%s: %.2f", label, *value);
  int th = msq__text_h (ctx);
  msq__text (ctx, r.x + MSQ_PAD, r.y + (r.h - th) / 2, buf, MSQ_COL_TEXT);

  ctx->tip_hot = hot ? id : ctx->tip_hot;
  msq__advance (ctx, r.w, r.h);
  return changed;
}

/* ---- text input ---------------------------------------------------------- */

bool msq_text_input (MsqCtx *ctx, const char *label, char *buf, int cap)
{
  MsqId id = msq__hash_str (ctx, label);
  if (label && *label)
    msq_label (ctx, label);

  MsqRect r = msq__next (ctx, -1, MSQ_ROW_H);
  if (!ctx->row_active)
    r.w = ctx->panel_w - (r.x - ctx->panel_x) - MSQ_PAD;

  bool hot = msq__hit (r, ctx->mouse_x, ctx->mouse_y);
  if (hot)
    ctx->hot = id;
  if (ctx->mouse_pressed)
    {
      if (hot)
        {
          ctx->focus = id;
          ctx->caret_owner = id;
          ctx->caret = (int) strlen (buf);
          SDL_StartTextInput ();
        }
      else if (ctx->focus == id)
        {
          ctx->focus = 0;
          SDL_StopTextInput ();
        }
    }

  bool changed = false;
  int len = (int) strlen (buf);

  if (ctx->focus == id)
    {
      if (ctx->caret > len) ctx->caret = len;
      /* insert typed text at caret */
      if (ctx->text_in_len > 0)
        {
          for (int i = 0; i < ctx->text_in_len && len + 1 < cap; i++)
            {
              memmove (buf + ctx->caret + 1, buf + ctx->caret,
                       (size_t) (len - ctx->caret + 1));
              buf[ctx->caret] = ctx->text_in[i];
              ctx->caret++;
              len++;
            }
          changed = true;
        }
      if (ctx->key_back && ctx->caret > 0)
        {
          memmove (buf + ctx->caret - 1, buf + ctx->caret,
                   (size_t) (len - ctx->caret + 1));
          ctx->caret--;
          changed = true;
        }
      if (ctx->key_left && ctx->caret > 0)
        ctx->caret--;
      if (ctx->key_right && ctx->caret < len)
        ctx->caret++;
      if (ctx->key_enter)
        {
          ctx->focus = 0;
          SDL_StopTextInput ();
        }
    }

  msq__fill (ctx, r, MSQ_COL_WIDGET_BG);
  msq__stroke (ctx, r, ctx->focus == id ? MSQ_COL_ACCENT : MSQ_COL_BORDER);
  int th = msq__text_h (ctx);
  int ty = r.y + (r.h - th) / 2;
  msq__text (ctx, r.x + MSQ_PAD, ty, buf, MSQ_COL_TEXT);

  if (ctx->focus == id)
    {
      /* caret: measure width of substring up to caret */
      char save = buf[ctx->caret];
      buf[ctx->caret] = 0;
      int cw = msq__text_w (ctx, buf);
      buf[ctx->caret] = save;
      if (((SDL_GetTicks () / 500) & 1) == 0)
        {
          MsqRect car = { r.x + MSQ_PAD + cw, ty, 2, th };
          msq__fill (ctx, car, MSQ_COL_TEXT);
        }
    }

  ctx->tip_hot = hot ? id : ctx->tip_hot;
  msq__advance (ctx, r.w, r.h);
  return changed;
}

/* ---- list (simple single-selection listbox with Y scroll) ---------------- */

/* find-or-create the retained scroll offset for a given list id */
static int *msq__scroll_off (MsqCtx *ctx, MsqId id)
{
  for (int i = 0; i < ctx->scroll_count; i++)
    if (ctx->scroll[i].id == id)
      return &ctx->scroll[i].off;
  if (ctx->scroll_count
      < (int) (sizeof ctx->scroll / sizeof *ctx->scroll))
    {
      ctx->scroll[ctx->scroll_count].id = id;
      ctx->scroll[ctx->scroll_count].off = 0;
      return &ctx->scroll[ctx->scroll_count++].off;
    }
  return NULL;
}

bool msq_list (MsqCtx *ctx, const char *label, const char *const *items,
               int count, int *selected, int visible_rows)
{
  if (label && *label)
    msq_label (ctx, label);

  MsqId list_id = msq__hash_str (ctx, label ? label : "list");
  int *off_p = msq__scroll_off (ctx, list_id);
  int off = off_p ? *off_p : 0;

  int row_h = MSQ_ROW_H + MSQ_GAP;            // stride per item
  int shown = (visible_rows > 0 && visible_rows < count)
                ? visible_rows : count;
  int view_h = shown * row_h - MSQ_GAP;       // visible content height
  int full_h = count * row_h - MSQ_GAP;       // total content height
  bool scrolls = full_h > view_h;

  // reserve the whole viewport rect so layout flows past the list
  int vw = ctx->panel_w - (ctx->cur_x - ctx->panel_x) - MSQ_PAD;
  if (ctx->row_active)
    vw = 160;
  MsqRect view = { ctx->cur_x, ctx->cur_y, vw, view_h };

  bool over = msq__hit (view, ctx->mouse_x, ctx->mouse_y);

  // wheel scrolling while hovering the list
  if (over && scrolls && ctx->wheel != 0)
    off -= ctx->wheel * row_h;

  int max_off = full_h - view_h;
  if (max_off < 0) max_off = 0;
  if (off < 0) off = 0;
  if (off > max_off) off = max_off;
  if (off_p) *off_p = off;

  // clip drawing to the viewport (records a clip op when inside a window)
  MsqRect clip = { view.x, view.y, view.w, view.h };
  msq__clip (ctx, &clip);

  bool changed = false;
  int th = msq__text_h (ctx);
  int bar_w = scrolls ? 3 : 0;                // thin scrollbar gutter

  for (int i = 0; i < count; i++)
    {
      msq_push_id (ctx, i);
      MsqId id = msq__hash_str (ctx, items[i]);

      MsqRect r = { view.x, view.y + i * row_h - off,
                    view.w - bar_w, MSQ_ROW_H };

      // only interact with rows actually inside the viewport
      bool visible = (r.y + r.h > view.y) && (r.y < view.y + view.h);
      bool hot = false, clicked = false;
      if (visible)
        clicked = msq__behave (ctx, id, r, &hot);

      if (clicked && *selected != i)
        {
          *selected = i;
          changed = true;
        }

      MsqColorSlot bg = MSQ_COL_WIDGET_BG;
      if (i == *selected)
        bg = MSQ_COL_ACCENT_DIM;
      else if (hot)
        bg = MSQ_COL_WIDGET_HOT;

      if (visible)
        {
          msq__fill (ctx, r, bg);
          msq__stroke (ctx, r, MSQ_COL_BORDER);
          msq__text (ctx, r.x + MSQ_PAD, r.y + (r.h - th) / 2, items[i],
                     MSQ_COL_TEXT);
        }
      msq_pop_id (ctx);
    }

  msq__clip (ctx, NULL);

  // thin scrollbar: a 3px track on the right with a proportional thumb
  if (scrolls)
    {
      MsqRect track = { view.x + view.w - 3, view.y, 3, view.h };
      msq__fill (ctx, track, MSQ_COL_WIDGET_BG);

      int thumb_h = view_h * view_h / full_h;
      if (thumb_h < 8) thumb_h = 8;
      int thumb_y = view.y + (view_h - thumb_h) * off / max_off;
      MsqRect thumb = { track.x, thumb_y, 3, thumb_h };
      msq__fill (ctx, thumb, MSQ_COL_ACCENT);
    }

  // advance layout cursor past the viewport
  msq__advance (ctx, view.w, view.h);
  return changed;
}

/* ---- textarea (multi-line editor with selection + clipboard) ------------- */

static int msq__imin (int a, int b) { return a < b ? a : b; }
static int msq__imax (int a, int b) { return a > b ? a : b; }

/* index of the first char on the line that contains index i */
static int msq__line_start (const char *buf, int i)
{
  while (i > 0 && buf[i - 1] != '\n') i--;
  return i;
}
/* index of the '\n' that ends the line containing i (or len at the last line) */
static int msq__line_end (const char *buf, int len, int i)
{
  while (i < len && buf[i] != '\n') i++;
  return i;
}
/* how many lines precede index i */
static int msq__row_of (const char *buf, int i)
{
  int r = 0;
  for (int k = 0; k < i; k++)
    if (buf[k] == '\n') r++;
  return r;
}
/* pixel width of buf[a..b); briefly NUL-terminates the user buffer */
static int msq__span_w (MsqCtx *ctx, char *buf, int a, int b)
{
  if (b <= a) return 0;
  char save = buf[b];
  buf[b] = 0;
  int w = msq__text_w (ctx, buf + a);
  buf[b] = save;
  return w;
}
/* nearest caret index for a pixel position inside the editor */
static int msq__ta_index_at (MsqCtx *ctx, char *buf, int len,
                             int text_x, int text_y, int line_h, int off,
                             int px, int py)
{
  int total = msq__row_of (buf, len) + 1;
  int row = (py - text_y + off) / line_h;
  if (row < 0) row = 0;
  if (row > total - 1) row = total - 1;
  int idx = 0, rr = 0;
  while (rr < row && idx < len) { if (buf[idx] == '\n') rr++; idx++; }
  int ls = idx, le = msq__line_end (buf, len, ls);
  int target = px - text_x, best = ls, bestd = 1 << 30;
  for (int k = ls; k <= le; k++)
    {
      int d = msq__span_w (ctx, buf, ls, k) - target;
      if (d < 0) d = -d;
      if (d < bestd) { bestd = d; best = k; }
    }
  return best;
}

/* delete the current selection (if any); returns whether anything changed */
static bool msq__sel_delete (char *buf, int *len, int *caret, int *anchor)
{
  int a = msq__imin (*caret, *anchor), b = msq__imax (*caret, *anchor);
  if (a == b) return false;
  memmove (buf + a, buf + b, (size_t) (*len - b + 1));
  *len -= (b - a);
  *caret = a;
  *anchor = a;
  return true;
}
/* replace the selection with insn bytes of ins (clamped to capacity) */
static bool msq__ta_insert (char *buf, int cap, int *len, int *caret,
                            int *anchor, const char *ins, int insn)
{
  bool changed = msq__sel_delete (buf, len, caret, anchor);
  int room = cap - 1 - *len;
  if (insn > room) insn = room;
  if (insn > 0)
    {
      memmove (buf + *caret + insn, buf + *caret,
               (size_t) (*len - *caret + 1));
      memcpy (buf + *caret, ins, (size_t) insn);
      *len += insn;
      *caret += insn;
      *anchor = *caret;
      changed = true;
    }
  return changed;
}

bool msq_textarea (MsqCtx *ctx, const char *label, char *buf, int cap,
                   int visible_rows)
{
  MsqId id = msq__hash_str (ctx, label);
  if (label && *label)
    msq_label (ctx, label);

  int th = msq__text_h (ctx);
  int line_h = th + 2;
  int rows = visible_rows > 0 ? visible_rows : 5;
  int box_h = rows * line_h + 2 * MSQ_PAD;

  MsqRect r = msq__next (ctx, -1, box_h);
  if (!ctx->row_active)
    r.w = ctx->panel_w - (r.x - ctx->panel_x) - MSQ_PAD;

  int text_x = r.x + MSQ_PAD;
  int text_y = r.y + MSQ_PAD;
  int len = (int) strlen (buf);

  bool hot = msq__hit (r, ctx->mouse_x, ctx->mouse_y);
  if (hot) ctx->hot = id;

  int *off_p = msq__scroll_off (ctx, id);
  int off = off_p ? *off_p : 0;

  /* focus follows clicks, just like msq_text_input */
  if (ctx->mouse_pressed)
    {
      if (hot)
        {
          ctx->focus = id;
          ctx->caret_owner = id;
          SDL_StartTextInput ();
        }
      else if (ctx->focus == id)
        {
          ctx->focus = 0;
          SDL_StopTextInput ();
        }
    }

  if (ctx->caret > len) ctx->caret = len;
  if (ctx->sel_anchor > len) ctx->sel_anchor = len;

  int caret_before = ctx->caret;
  bool changed = false;
  bool shift = (SDL_GetModState () & KMOD_SHIFT) != 0;

  /* mouse: click places the caret + anchor, drag extends the selection */
  if (ctx->focus == id)
    {
      if (ctx->mouse_pressed && hot)
        {
          int bi = msq__ta_index_at (ctx, buf, len, text_x, text_y,
                                     line_h, off, ctx->mouse_x, ctx->mouse_y);
          ctx->caret = bi;
          if (!shift) ctx->sel_anchor = bi;
          ctx->active = id;
        }
      else if (ctx->active == id && ctx->mouse_down)
        {
          ctx->caret = msq__ta_index_at (ctx, buf, len, text_x, text_y,
                                         line_h, off, ctx->mouse_x, ctx->mouse_y);
        }
      if (ctx->active == id && ctx->mouse_released)
        ctx->active = 0;
    }

  /* keyboard editing + navigation + clipboard */
  if (ctx->focus == id)
    {
      int caret = ctx->caret, anchor = ctx->sel_anchor;

      if (ctx->text_in_len > 0)
        changed |= msq__ta_insert (buf, cap, &len, &caret, &anchor,
                                   ctx->text_in, ctx->text_in_len);
      if (ctx->key_enter)
        {
          char nl = '\n';
          changed |= msq__ta_insert (buf, cap, &len, &caret, &anchor, &nl, 1);
        }
      if (ctx->key_back)
        {
          if (caret != anchor)
            changed |= msq__sel_delete (buf, &len, &caret, &anchor);
          else if (caret > 0)
            {
              memmove (buf + caret - 1, buf + caret, (size_t) (len - caret + 1));
              caret--; len--; anchor = caret; changed = true;
            }
        }
      if (ctx->key_del)
        {
          if (caret != anchor)
            changed |= msq__sel_delete (buf, &len, &caret, &anchor);
          else if (caret < len)
            {
              memmove (buf + caret, buf + caret + 1, (size_t) (len - caret));
              len--; anchor = caret; changed = true;
            }
        }
      if (ctx->key_copy || ctx->key_cut)
        {
          int a = msq__imin (caret, anchor), b = msq__imax (caret, anchor);
          if (b > a)
            {
              char save = buf[b];
              buf[b] = 0;
              SDL_SetClipboardText (buf + a);
              buf[b] = save;
              if (ctx->key_cut)
                changed |= msq__sel_delete (buf, &len, &caret, &anchor);
            }
        }
      if (ctx->key_paste)
        {
          char *cb = SDL_GetClipboardText ();
          if (cb && *cb)
            changed |= msq__ta_insert (buf, cap, &len, &caret, &anchor,
                                       cb, (int) strlen (cb));
          if (cb) SDL_free (cb);
        }
      if (ctx->key_selectall) { anchor = 0; caret = len; }

      bool sel = (caret != anchor);
      if (ctx->key_left)
        {
          if (shift) { if (caret > 0) caret--; }
          else if (sel) caret = msq__imin (caret, anchor);
          else if (caret > 0) caret--;
          if (!shift) anchor = caret;
        }
      if (ctx->key_right)
        {
          if (shift) { if (caret < len) caret++; }
          else if (sel) caret = msq__imax (caret, anchor);
          else if (caret < len) caret++;
          if (!shift) anchor = caret;
        }
      if (ctx->key_home)
        { caret = msq__line_start (buf, caret); if (!shift) anchor = caret; }
      if (ctx->key_end)
        { caret = msq__line_end (buf, len, caret); if (!shift) anchor = caret; }
      if (ctx->key_up || ctx->key_down)
        {
          int ls = msq__line_start (buf, caret);
          int col = caret - ls;
          if (ctx->key_up && ls > 0)
            {
              int pls = msq__line_start (buf, ls - 1);
              caret = pls + msq__imin (col, ls - 1 - pls);
            }
          else if (ctx->key_down)
            {
              int le = msq__line_end (buf, len, caret);
              if (le < len)
                {
                  int nls = le + 1, nle = msq__line_end (buf, len, nls);
                  caret = nls + msq__imin (col, nle - nls);
                }
            }
          if (!shift) anchor = caret;
        }

      if (caret < 0) caret = 0;
      if (caret > len) caret = len;
      ctx->caret = caret;
      ctx->sel_anchor = anchor;
    }

  /* scrolling: wheel while hovering, but snap to the caret when it moves */
  int total_rows = msq__row_of (buf, len) + 1;
  if (hot && ctx->wheel != 0)
    off -= ctx->wheel * line_h * 3;
  if (ctx->focus == id && (changed || ctx->caret != caret_before))
    {
      int crow = msq__row_of (buf, ctx->caret);
      int first = off / line_h;
      if (crow < first) off = crow * line_h;
      else if (crow >= first + rows) off = (crow - rows + 1) * line_h;
    }
  int max_off = (total_rows - rows) * line_h;
  if (max_off < 0) max_off = 0;
  if (off > max_off) off = max_off;
  if (off < 0) off = 0;
  if (off_p) *off_p = off;

  /* draw: frame, then clipped text + selection, then a thin scrollbar */
  msq__fill (ctx, r, MSQ_COL_WIDGET_BG);
  msq__stroke (ctx, r, ctx->focus == id ? MSQ_COL_ACCENT : MSQ_COL_BORDER);

  MsqRect clip = { r.x + 1, r.y + 1, r.w - 2, r.h - 2 };
  msq__clip (ctx, &clip);

  int a = msq__imin (ctx->caret, ctx->sel_anchor);
  int b = msq__imax (ctx->caret, ctx->sel_anchor);
  int first_row = off / line_h;
  int idx = 0, rr = 0;
  while (rr < first_row && idx < len) { if (buf[idx] == '\n') rr++; idx++; }

  for (int row = first_row; row <= first_row + rows && idx <= len; row++)
    {
      int ls = idx, le = msq__line_end (buf, len, ls);
      int ly = text_y + row * line_h - off;

      /* selection highlight: this line (and its '\n' if the run continues) */
      if (b > a && a <= le && b > ls)
        {
          int s0 = a > ls ? a : ls;
          int s1 = b < le ? b : le;
          int x0 = text_x + msq__span_w (ctx, buf, ls, s0);
          int w  = (text_x + msq__span_w (ctx, buf, ls, s1)) - x0;
          if (b > le) w += 6;          /* the newline is part of the selection */
          if (w > 0)
            {
              MsqRect hl = { x0, ly, w, line_h };
              msq__fill (ctx, hl, MSQ_COL_ACCENT_DIM);
            }
        }

      /* the line's text */
      char save = buf[le];
      buf[le] = 0;
      msq__text (ctx, text_x, ly, buf + ls, MSQ_COL_TEXT);
      buf[le] = save;

      if (le >= len) break;            /* last line drawn */
      idx = le + 1;
    }

  /* caret */
  if (ctx->focus == id && ((SDL_GetTicks () / 500) & 1) == 0)
    {
      int crow = msq__row_of (buf, ctx->caret);
      int cls  = msq__line_start (buf, ctx->caret);
      int cx   = text_x + msq__span_w (ctx, buf, cls, ctx->caret);
      int cy   = text_y + crow * line_h - off;
      MsqRect car = { cx, cy, 2, th };
      msq__fill (ctx, car, MSQ_COL_TEXT);
    }

  msq__clip (ctx, NULL);

  if (total_rows > rows)
    {
      int view_h = r.h, full_h = total_rows * line_h;
      MsqRect track = { r.x + r.w - 3, r.y, 3, r.h };
      msq__fill (ctx, track, MSQ_COL_WIDGET_BG);
      int thumb_h = view_h * (rows * line_h) / full_h;
      if (thumb_h < 8) thumb_h = 8;
      int thumb_y = r.y + (max_off ? (view_h - thumb_h) * off / max_off : 0);
      MsqRect thumb = { track.x, thumb_y, 3, thumb_h };
      msq__fill (ctx, thumb, MSQ_COL_ACCENT);
    }

  ctx->tip_hot = hot ? id : ctx->tip_hot;
  msq__advance (ctx, r.w, r.h);
  return changed;
}

/* ---- tab group ----------------------------------------------------------- */

bool msq_tabs (MsqCtx *ctx, const char *label, const char *const *tabs,
               int count, int *selected)
{
  if (label && *label)
    msq_label (ctx, label);

  int h  = MSQ_ROW_H;
  int th = msq__text_h (ctx);
  int x  = ctx->cur_x;
  int y  = ctx->cur_y;
  bool changed = false;

  for (int i = 0; i < count; i++)
    {
      msq_push_id (ctx, i);
      MsqId id = msq__hash_str (ctx, tabs[i]);
      int tw = msq__text_w (ctx, tabs[i]);
      MsqRect r = { x, y, tw + 2 * MSQ_PAD + 6, h };

      bool hot;
      bool clicked = msq__behave (ctx, id, r, &hot);
      if (clicked && *selected != i) { *selected = i; changed = true; }

      bool sel = (i == *selected);
      MsqColorSlot bg = sel ? MSQ_COL_WIDGET_ACTIVE
                            : (hot ? MSQ_COL_WIDGET_HOT : MSQ_COL_WIDGET_BG);
      msq__fill   (ctx, r, bg);
      msq__stroke (ctx, r, MSQ_COL_BORDER);
      if (sel)
        {
          MsqRect bar = { r.x, r.y + r.h - 2, r.w, 2 };   /* accent underline */
          msq__fill (ctx, bar, MSQ_COL_ACCENT);
        }
      msq__text (ctx, r.x + (r.w - tw) / 2, r.y + (h - th) / 2, tabs[i],
                 sel ? MSQ_COL_TEXT : MSQ_COL_TEXT_DIM);

      ctx->tip_hot = hot ? id : ctx->tip_hot;
      x += r.w + 2;          /* small gap between tabs */
      msq_pop_id (ctx);
    }

  /* advance the layout cursor past the strip */
  if (ctx->row_active)
    {
      ctx->cur_x = x;
      if (h > ctx->row_max_h) ctx->row_max_h = h;
    }
  else
    {
      ctx->cur_x = ctx->panel_x + MSQ_PAD;
      ctx->cur_y = y + h + MSQ_GAP;
    }
  return changed;
}

/* ---- popup menu (deferred draw) ----------------------------------------- */

static MsqPopupState *msq__popup_find (MsqCtx *ctx, MsqId id)
{
  for (int i = 0; i < ctx->popup_count; i++)
    if (ctx->popups[i].id == id)
      return &ctx->popups[i];
  return NULL;
}

void msq_popup_open (MsqCtx *ctx, MsqId id, int x, int y)
{
  MsqPopupState *p = msq__popup_find (ctx, id);
  if (!p && ctx->popup_count < (int) (sizeof ctx->popups / sizeof *ctx->popups))
    p = &ctx->popups[ctx->popup_count++];
  if (p)
    {
      p->id = id;
      p->x = x;
      p->y = y;
      p->open = true;
    }
}

bool msq_popup (MsqCtx *ctx, MsqId id, const char *const *items, int count,
                int *out_choice)
{
  MsqPopupState *p = msq__popup_find (ctx, id);
  if (!p || !p->open)
    return false;

  /* size to widest item */
  int w = 60, ih = MSQ_ROW_H;
  for (int i = 0; i < count; i++)
    {
      int tw = msq__text_w (ctx, items[i]) + 2 * MSQ_PAD * 2;
      if (tw > w)
        w = tw;
    }
  int h = count * ih + 2;
  MsqRect box = { p->x, p->y, w, h };

  /* dismiss on click-outside */
  bool inside = msq__hit (box, ctx->mouse_x, ctx->mouse_y);
  if (ctx->mouse_pressed && !inside)
    {
      p->open = false;
      return false;
    }

  /* The popup is modal over whatever sits beneath it: while the cursor is on
   * the popup, claim the active slot so widgets that are declared later this
   * frame and overlapped by the popup do not also react to the same click.
   * Released below, once the choice has been read. */
  if (inside && ctx->mouse_pressed)
    ctx->active = id;

  /* Hit-test now (the mouse snapshot is current) but defer the drawing to
   * msq_end_frame so the popup paints above any later widgets. */
  bool chose = false;
  for (int i = 0; i < count; i++)
    {
      MsqRect it = { box.x + 1, box.y + 1 + i * ih, w - 2, ih };
      if (msq__hit (it, ctx->mouse_x, ctx->mouse_y) && ctx->mouse_released)
        {
          if (out_choice)
            *out_choice = i;
          chose = true;
          p->open = false;
        }
    }
  if (ctx->active == id && ctx->mouse_released)
    ctx->active = 0;

  if (p->open
      && ctx->popup_draw_count
           < (int) (sizeof ctx->popup_draws / sizeof *ctx->popup_draws))
    {
      MsqPopupDraw *d = &ctx->popup_draws[ctx->popup_draw_count++];
      d->box = box;
      d->items = items;
      d->count = count;
    }
  return chose;
}

/* ---- dropdown ------------------------------------------------------------ */

bool msq_dropdown (MsqCtx *ctx, const char *label, const char *const *items,
                   int count, int *selected)
{
  MsqId id = msq__hash_str (ctx, label);
  MsqId pid = id ^ 0x9e3779b9u; /* derived popup id, stable per call site */

  if (label && *label)
    msq_label (ctx, label);

  const char *shown = (*selected >= 0 && *selected < count)
                        ? items[*selected] : "Select...";
  int w = msq__text_w (ctx, shown) + 2 * MSQ_PAD * 2 + 18;
  MsqRect r = msq__next (ctx, ctx->row_active ? w : -1, MSQ_ROW_H);
  if (!ctx->row_active)
    r.w = ctx->panel_w - (r.x - ctx->panel_x) - MSQ_PAD;

  bool hot;
  bool clicked = msq__behave (ctx, id, r, &hot);
  if (clicked)
    msq_popup_open (ctx, pid, r.x, r.y + r.h);

  msq__fill (ctx, r, hot ? MSQ_COL_WIDGET_HOT : MSQ_COL_WIDGET_BG);
  msq__stroke (ctx, r, MSQ_COL_BORDER);
  int th = msq__text_h (ctx);
  msq__text (ctx, r.x + MSQ_PAD, r.y + (r.h - th) / 2, shown, MSQ_COL_TEXT);
  /* caret: filled triangle that flips 180 deg while the popup is open */
  int tri = 8;
  int tx = r.x + r.w - MSQ_PAD - tri;
  int ty = r.y + (r.h - tri) / 2;
  MsqPopupState *pst = msq__popup_find (ctx, pid);
  //draw_triangle_rotated (ctx->ren, tx, ty, tri, (pst && pst->open) ? 2 : 0);
  msq__tri_angled (ctx, tx, ty, 6, (pst && pst->open) ? 0 : 180);

  ctx->tip_hot = hot ? id : ctx->tip_hot;
  msq__advance (ctx, r.w, r.h);

  /* the popup is drawn during this same widget call but on top of the panel;
   * because mosquito draws in submission order, declare dropdowns after other
   * widgets if overlap is a concern, or rely on end_frame layering. */
  int choice = -1;
  if (msq_popup (ctx, pid, items, count, &choice))
    {
      if (choice != *selected)
        {
          *selected = choice;
          return true;
        }
    }
  return false;
}

/* ---- tooltip ------------------------------------------------------------- */

void msq_tooltip (MsqCtx *ctx, const char *text)
{
  /* attach to the most recently hot widget id captured by the prior call */
  if (ctx->hot != 0 && ctx->hot == ctx->tip_hot)
    {
      if (ctx->tip_text[0] == 0 || strcmp (ctx->tip_text, text) != 0
          || ctx->tip_since == 0)
        {
          if (ctx->tip_since == 0)
            ctx->tip_since = SDL_GetTicks ();
          SDL_strlcpy (ctx->tip_text, text, sizeof ctx->tip_text);
        }
      if (SDL_GetTicks () - ctx->tip_since >= MSQ_TOOLTIP_DELAY_MS)
        {
          ctx->tip_pending = true;
          ctx->tip_x = ctx->mouse_x + 14;
          ctx->tip_y = ctx->mouse_y + 18;
        }
    }
  else
    {
      ctx->tip_since = 0;
    }
}

/* ---- child windows ------------------------------------------------------- */

static MsqWindow *msq__win_find (MsqCtx *ctx, MsqId id)
{
  for (int i = 0; i < ctx->win_count; i++)
    if (ctx->windows[i].id == id)
      return &ctx->windows[i];
  return NULL;
}

bool msq_window_begin (MsqCtx *ctx, const char *title, int x, int y,
                       int w, int h, bool *open)
{
  MsqId id = msq_gen_id (ctx, title);   /* stable across frames, no line mixing */
  MsqWindow *win = msq__win_find (ctx, id);
  if (!win)
    {
      if (ctx->win_count >= MSQ_MAX_WINDOWS)
        return false;
      win = &ctx->windows[ctx->win_count++];
      memset (win, 0, sizeof *win);
      win->id   = id;
      win->rect = (MsqRect){ x, y, w, h };
      win->open = true;
      win->z    = ++ctx->z_counter;
    }

  if (open)
    win->open = *open;       /* when supplied, the caller's bool is the truth */
  if (!win->open)
    return false;

  win->used = true;
  bool is_front = (id == ctx->front_win);
  bool closable = (open != NULL);

  MsqRect wr = win->rect;
  MsqRect tb0   = { wr.x, wr.y, wr.w, MSQ_TITLEBAR_H };
  MsqRect cb0   = { wr.x + wr.w - MSQ_CLOSE_SIZE - MSQ_CLOSE_PAD,
                    wr.y + (MSQ_TITLEBAR_H - MSQ_CLOSE_SIZE) / 2,
                    MSQ_CLOSE_SIZE, MSQ_CLOSE_SIZE };
  MsqRect grip  = { wr.x + wr.w - MSQ_RESIZE_GRIP, wr.y + wr.h - MSQ_RESIZE_GRIP,
                    MSQ_RESIZE_GRIP, MSQ_RESIZE_GRIP };
  bool hov_cb   = closable && is_front && msq__hit (cb0, ctx->mouse_x, ctx->mouse_y);
  bool hov_tb   = is_front && msq__hit (tb0, ctx->mouse_x, ctx->mouse_y);
  bool hov_grip = is_front && msq__hit (grip, ctx->mouse_x, ctx->mouse_y);

  /* close button: only the front window can be hit */
  if (hov_cb && ctx->mouse_pressed)
    {
      win->open = false;
      win->dragging = win->resizing = false;
      if (open) *open = false;
      return false;
    }

  /* a press raises the window; on the grip it resizes, on the title bar it drags */
  if (is_front && ctx->mouse_pressed
      && msq__hit (wr, ctx->mouse_x, ctx->mouse_y))
    {
      win->z = ++ctx->z_counter;
      ctx->front_win = id;
      if (hov_grip)
        {
          win->resizing = true;
          win->drag_off_x = ctx->mouse_x - (wr.x + wr.w);
          win->drag_off_y = ctx->mouse_y - (wr.y + wr.h);
        }
      else if (hov_tb && !hov_cb)
        {
          win->dragging = true;
          win->drag_off_x = ctx->mouse_x - wr.x;
          win->drag_off_y = ctx->mouse_y - wr.y;
        }
    }
  if (win->dragging)
    {
      if (ctx->mouse_down)
        {
          win->rect.x = ctx->mouse_x - win->drag_off_x;
          win->rect.y = ctx->mouse_y - win->drag_off_y;
        }
      else
        win->dragging = false;
    }
  if (win->resizing)
    {
      if (ctx->mouse_down)
        {
          int nw = ctx->mouse_x - win->rect.x - win->drag_off_x;
          int nh = ctx->mouse_y - win->rect.y - win->drag_off_y;
          win->rect.w = nw < MSQ_WIN_MIN_W ? MSQ_WIN_MIN_W : nw;
          win->rect.h = nh < MSQ_WIN_MIN_H ? MSQ_WIN_MIN_H : nh;
        }
      else
        win->resizing = false;
    }

  /* geometry may have moved/resized -- recompute before drawing */
  wr = win->rect;
  MsqRect tb = { wr.x, wr.y, wr.w, MSQ_TITLEBAR_H };
  MsqRect cb = { wr.x + wr.w - MSQ_CLOSE_SIZE - MSQ_CLOSE_PAD,
                 wr.y + (MSQ_TITLEBAR_H - MSQ_CLOSE_SIZE) / 2,
                 MSQ_CLOSE_SIZE, MSQ_CLOSE_SIZE };

  /* start recording this window's draw ops under its own z */
  ctx->cur_win   = win;
  ctx->recording = true;
  ctx->rec_z     = win->z;
  ctx->win_grip_hot = hov_grip;
  win->cmd_start = ctx->cmd_count;

  /* decorations drawn before the body clip so they stay crisp: shadow, body
   * fill, title bar, title, close button. The border + grip come in window_end,
   * on top of the (clipped) content. */
  msq__shadow_rec (ctx, wr);
  MsqRect body = { wr.x, wr.y + MSQ_TITLEBAR_H, wr.w, wr.h - MSQ_TITLEBAR_H };
  msq__fill_c (ctx, body, ctx->pal[MSQ_COL_WINDOW_BODY]);
  msq__fill_c (ctx, tb,
               ctx->pal[is_front ? MSQ_COL_TITLEBAR_ACTIVE : MSQ_COL_TITLEBAR]);

  /* title uses the regular widget font (TTF by default); the rect font is
   * reserved for places that genuinely need a tiny, no-blur caption */
  int tth = msq__text_h (ctx);
  int tty = wr.y + (MSQ_TITLEBAR_H - tth) / 2;
  msq__text (ctx, wr.x + MSQ_PAD, tty, title, MSQ_COL_TEXT);

  if (closable)
    {
      msq__fill_c (ctx, cb, ctx->pal[hov_cb ? MSQ_COL_CLOSE_HOT : MSQ_COL_CLOSE]);
      MsqColor xcol = ctx->pal[MSQ_COL_TEXT];
      int m = 4;
      msq__line (ctx, cb.x + m, cb.y + m,
                 cb.x + cb.w - m - 1, cb.y + cb.h - m - 1, xcol);
      msq__line (ctx, cb.x + m, cb.y + cb.h - m - 1,
                 cb.x + cb.w - m - 1, cb.y + m, xcol);
    }

  /* clip what follows to the body interior so content can never spill outside
   * the window (notably once it has been resized smaller) */
  ctx->clip_base = (MsqRect){ wr.x + 1, wr.y + MSQ_TITLEBAR_H,
                              wr.w - 2, wr.h - MSQ_TITLEBAR_H - 1 };
  ctx->clip_active = true;
  msq__clip (ctx, NULL);          /* emit the body clip */

  /* lay widgets out inside the body just like a panel */
  ctx->panel_x = wr.x;
  ctx->panel_y = wr.y + MSQ_TITLEBAR_H;
  ctx->panel_w = wr.w;
  ctx->cur_x   = wr.x + MSQ_PAD;
  ctx->cur_y   = wr.y + MSQ_TITLEBAR_H + MSQ_PAD;
  ctx->row_active = 0;

  /* gate body input: only the front window sees the real cursor, and not while
   * it is being dragged or resized */
  ctx->win_saved_mx = ctx->mouse_x;
  ctx->win_saved_my = ctx->mouse_y;
  if (!is_front || win->dragging || win->resizing)
    {
      ctx->mouse_x = -100000;
      ctx->mouse_y = -100000;
    }
  return true;
}

void msq_window_end (MsqCtx *ctx)
{
  MsqWindow *win = ctx->cur_win;
  if (!win)
    return;

  /* lift the body clip, then draw the border + resize grip over the content */
  ctx->clip_active = false;
  msq__clip (ctx, NULL);

  MsqRect wr = win->rect;
  msq__stroke_c (ctx, wr, ctx->pal[MSQ_COL_BORDER]);

  /* resize grip: a small triangle tucked into the bottom-right corner */
  {
    bool active = win->resizing || ctx->win_grip_hot;
    MsqColor gc = ctx->pal[active ? MSQ_COL_ACCENT : MSQ_COL_TEXT_DIM];
    int pad = 4, ts = MSQ_RESIZE_GRIP - 2 * pad + 2;   /* triangle size */
    int gx = wr.x + wr.w - pad - ts;
    int gy = wr.y + wr.h - pad - ts;
    for (int k = 0; k < ts; k++)
      {
        MsqRect ln = { gx + ts - (k + 1), gy + k, k + 1, 1 };
        msq__fill_c (ctx, ln, gc);
      }
  }

  win->cmd_count = ctx->cmd_count - win->cmd_start;
  ctx->recording = false;
  ctx->cur_win   = NULL;
  ctx->mouse_x   = ctx->win_saved_mx;
  ctx->mouse_y   = ctx->win_saved_my;
}

/* replay one recorded command immediately onto the renderer */
static void msq__replay (MsqCtx *ctx, const MsqCmd *c)
{
  switch (c->kind)
    {
    case MSQ_CMD_FILL:
      {
        SDL_Rect rr = { c->a, c->b, c->c, c->d };
        SDL_SetRenderDrawColor (ctx->ren, c->color.r, c->color.g, c->color.b, c->color.a);
        SDL_RenderFillRect (ctx->ren, &rr);
      }
      break;
    case MSQ_CMD_STROKE:
      {
        SDL_Rect rr = { c->a, c->b, c->c, c->d };
        SDL_SetRenderDrawColor (ctx->ren, c->color.r, c->color.g, c->color.b, c->color.a);
        SDL_RenderDrawRect (ctx->ren, &rr);
      }
      break;
    case MSQ_CMD_TEXT:
      msq__text_draw (ctx, c->a, c->b, c->text, c->color, false, c->e);
      break;
    case MSQ_CMD_RTEXT:
      msq__text_draw (ctx, c->a, c->b, c->text, c->color, true, c->e);
      break;
    case MSQ_CMD_LINE:
      SDL_SetRenderDrawColor (ctx->ren, c->color.r, c->color.g, c->color.b, c->color.a);
      SDL_RenderDrawLine (ctx->ren, c->a, c->b, c->c, c->d);
      break;
    case MSQ_CMD_CLIP:
      if (c->e)
        {
          SDL_Rect cl = { c->a, c->b, c->c, c->d };
          SDL_RenderSetClipRect (ctx->ren, &cl);
        }
      else
        SDL_RenderSetClipRect (ctx->ren, NULL);
      break;
    case MSQ_CMD_TRI:
      draw_triangle_angled (ctx->ren, c->a, c->b, c->c, c->e);
      break;
    case MSQ_CMD_SHADOW:
      {
        SDL_Rect rr = { c->a, c->b, c->c, c->d };
        draw_shadow_simple (ctx->ren, rr);
      }
      break;
    }
}

/* replay all windows back-to-front so the focused one paints on top */
static void msq__flush_windows (MsqCtx *ctx)
{
  int order[MSQ_MAX_WINDOWS];
  int n = 0;
  for (int i = 0; i < ctx->win_count; i++)
    if (ctx->windows[i].used)
      order[n++] = i;

  /* stable insertion sort by z ascending (few windows, tiny n) */
  for (int i = 1; i < n; i++)
    {
      int k = order[i], j = i - 1;
      while (j >= 0 && ctx->windows[order[j]].z > ctx->windows[k].z)
        { order[j + 1] = order[j]; j--; }
      order[j + 1] = k;
    }

  SDL_RenderSetClipRect (ctx->ren, NULL);
  for (int oi = 0; oi < n; oi++)
    {
      MsqWindow *win = &ctx->windows[order[oi]];
      for (int ci = 0; ci < win->cmd_count; ci++)
        msq__replay (ctx, &ctx->cmds[win->cmd_start + ci]);
    }
  SDL_RenderSetClipRect (ctx->ren, NULL);
}

void msq_end_frame (MsqCtx *ctx)
{
  /* child windows first: replayed bottom-to-top, above the top-level widgets
   * but below popups and tooltips. */
  msq__flush_windows (ctx);

  /* deferred popup bodies: drawn here so they sit above every widget that was
   * declared after the popup during the frame (e.g. a dropdown's list over the
   * buttons beneath it). Submission order is preserved between popups. */
  for (int pi = 0; pi < ctx->popup_draw_count; pi++)
    {
      MsqPopupDraw *d = &ctx->popup_draws[pi];
      SDL_Rect shadow = { d->box.x, d->box.y, d->box.w, d->box.h };
      draw_shadow_simple (ctx->ren, shadow);
      msq__fill (ctx, d->box, MSQ_COL_POPUP_BG);
      msq__stroke (ctx, d->box, MSQ_COL_BORDER);
      int ih = MSQ_ROW_H, th = msq__text_h (ctx);
      for (int i = 0; i < d->count; i++)
        {
          MsqRect it = { d->box.x + 1, d->box.y + 1 + i * ih, d->box.w - 2, ih };
          if (msq__hit (it, ctx->mouse_x, ctx->mouse_y))
            msq__fill (ctx, it, MSQ_COL_WIDGET_HOT);
          msq__text (ctx, it.x + MSQ_PAD, it.y + (ih - th) / 2, d->items[i],
                     MSQ_COL_TEXT);
        }
    }

  /* deferred tooltip layer, drawn last so it sits above everything */
  if (ctx->tip_pending && ctx->tip_text[0])
    {
      int tw = msq__text_w (ctx, ctx->tip_text);
      int th = msq__text_h (ctx);
      MsqRect r = { ctx->tip_x, ctx->tip_y, tw + 2 * MSQ_PAD, th + MSQ_PAD };
      SDL_Rect shadow = { r.x, r.y, r.w, r.h };
      draw_shadow_simple(ctx->ren, shadow);
      msq__fill (ctx, r, MSQ_COL_TOOLTIP_BG);
      msq__stroke (ctx, r, MSQ_COL_ACCENT_DIM);
      msq__text (ctx, r.x + MSQ_PAD, r.y + MSQ_PAD / 2, ctx->tip_text, MSQ_COL_TEXT);
      
    }

  /* consume per-frame edge events */
  ctx->mouse_pressed = false;
  ctx->mouse_released = false;
  ctx->mouse_pressed_r = false;
  ctx->wheel = 0;
  ctx->text_in_len = 0;
  ctx->text_in[0] = 0;
  ctx->key_back = ctx->key_left = ctx->key_right = false;
  ctx->key_enter = ctx->key_tab = false;
  ctx->key_up = ctx->key_down = ctx->key_home = ctx->key_end = false;
  ctx->key_del = false;
  ctx->key_copy = ctx->key_cut = ctx->key_paste = ctx->key_selectall = false;
}

#endif /* MSQ_IMPLEMENTATION */
#endif /* MOSQUITO_H */
