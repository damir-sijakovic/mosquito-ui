# mosquito — Documentation

The complete manual for **mosquito.h**, a single-header immediate-mode GUI for
C + SDL2. For a high-level overview and quick start, see
[README.md](README.md); this document is the reference.

Conventions: every public function is `msq_*`, every type is `MsqFoo`, every enum
value is `MSQ_FOO`. All sizes are in pixels. Indices are 0-based.

---

## Contents

- [1. Mental model](#1-mental-model)
- [2. Setup and lifecycle](#2-setup-and-lifecycle)
- [3. The frame loop](#3-the-frame-loop)
- [4. Event handling](#4-event-handling)
- [5. Layout](#5-layout)
- [6. Widgets](#6-widgets)
  - [6.1 Button](#61-button)
  - [6.2 Checkbox](#62-checkbox)
  - [6.3 Slider](#63-slider)
  - [6.4 Text input](#64-text-input)
  - [6.5 Textarea](#65-textarea)
  - [6.6 List](#66-list)
  - [6.7 Dropdown](#67-dropdown)
  - [6.8 Popup menu](#68-popup-menu)
  - [6.9 Tabs](#69-tabs)
  - [6.10 Tooltip](#610-tooltip)
- [7. Child windows](#7-child-windows)
- [8. Widget identity](#8-widget-identity)
- [9. Fonts](#9-fonts)
- [10. Theming and colors](#10-theming-and-colors)
- [11. Standalone drawing helpers](#11-standalone-drawing-helpers)
- [12. Compile-time configuration](#12-compile-time-configuration)
- [13. How it works inside](#13-how-it-works-inside)
- [14. Building and integrating](#14-building-and-integrating)
- [15. FAQ and gotchas](#15-faq-and-gotchas)
- [16. API cheat sheet](#16-api-cheat-sheet)

---

## 1. Mental model

mosquito is **immediate mode**. There is no retained widget tree: every frame you
*call* widget functions in order, and each call both draws the widget and returns
its interaction result for that frame. A button is "pressed" because
`msq_button(...)` returned `true` this frame, not because a `Button` object fired
a callback.

The only thing that survives between frames is a single opaque context,
`MsqCtx`, which holds:

- the **hot** widget (hovered), the **active** widget (pressed/dragged), and the
  **focus** widget (keyboard);
- the text caret index and selection anchor for the focused editor;
- open **popups** and their positions;
- the **tooltip** hover timer;
- per-list / per-textarea scroll offsets;
- **child-window** geometry, z-order, drag/resize state, and open flags;
- a per-frame **draw-command buffer** used to layer windows correctly.

You create one context, reuse it forever, and destroy it at shutdown.

---

## 2. Setup and lifecycle

```c
MsqCtx *msq_create (SDL_Renderer *renderer, const char *ttf_path, int font_px);
void    msq_destroy(MsqCtx *ctx);
```

- `renderer` — an `SDL_Renderer` you own. mosquito enables
  `SDL_BLENDMODE_BLEND` on it so translucent colors (tooltips, popups, shadows)
  composite correctly.
- `ttf_path` — path to a `.ttf`/`.otf` for crisp text, or `NULL`. With `NULL`
  (or when the file fails to open, or when built with `MSQ_NO_TTF`), widgets use
  the built-in [rect font](#9-fonts) instead of drawing blank.
- `font_px` — TTF pixel size (defaults to 16 if ≤ 0). Also seeds the rect-font
  scale (3 if `font_px ≥ 18`, else 2).

`msq_create` calls `TTF_Init()` if needed (unless `MSQ_NO_TTF`). It does **not**
call `SDL_Init` or create a window/renderer — that's your job. `msq_destroy`
closes the font and frees the context.

```c
SDL_Init(SDL_INIT_VIDEO);
SDL_Window   *win = SDL_CreateWindow("app", 0, 0, 800, 600, SDL_WINDOW_SHOWN);
SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
MsqCtx *ui = msq_create(ren, "/path/to/font.ttf", 16);
// ... frame loop ...
msq_destroy(ui);
```

---

## 3. The frame loop

```c
void msq_begin_frame(MsqCtx *ctx);
void msq_end_frame  (MsqCtx *ctx);
```

Every frame has three phases:

```c
msq_begin_frame(ui);                 // 1. reset per-frame state
while (SDL_PollEvent(&e))
    msq_handle_event(ui, &e);        // 2. feed events
// 3. clear renderer, declare widgets + windows
msq_end_frame(ui);                   // 4. flush deferred layers
SDL_RenderPresent(ren);
```

- **`msq_begin_frame`** clears the hot widget, resets the layout cursor and the
  per-frame id counter, empties the window command buffer, and computes which
  child window is front-most under the cursor (used for input routing this
  frame). Call it before polling events.
- **`msq_end_frame`** replays the recorded child windows in z-order, then draws
  popups and tooltips on top, then consumes the frame's edge events (mouse
  press/release/wheel, typed text, key edges). Call it last, before
  `SDL_RenderPresent`.

mosquito never clears or presents the screen. Clear it yourself (often to
`MSQ_COL_WINDOW_BG`) before declaring widgets, and present after `msq_end_frame`.

**Layering, bottom to top:** top-level (panel) widgets → child windows (by z) →
popups → tooltips.

---

## 4. Event handling

```c
void msq_handle_event(MsqCtx *ctx, const SDL_Event *e);
```

Call this once for every event you poll, between `begin` and `end`. mosquito
reads only what it needs and ignores the rest, so it coexists with your own
event handling. It records:

- **Mouse**: motion, left/right button down/up, wheel.
- **Text**: `SDL_TEXTINPUT` (UTF-8), accumulated for the focused editor.
- **Keys** (on `SDL_KEYDOWN`): Backspace, Delete, Left, Right, Up, Down, Home,
  End, Return/Keypad-Enter, Tab, and — when Ctrl is held — `C`, `X`, `V`, `A`
  (copy/cut/paste/select-all for the textarea).

Modifier state for Shift selection is read live via `SDL_GetModState()` inside
the textarea, so you don't need to forward modifiers specially.

Edge events (press/release/wheel/typed text/key edges) are cleared at the end of
`msq_end_frame`, after every widget has had a chance to read them. Level state
(mouse button held, cursor position) persists.

---

## 5. Layout

Widgets are placed by an auto-advancing cursor, not explicit rectangles.

```c
void msq_panel_begin(MsqCtx *ctx, int x, int y, int w);
void msq_panel_end  (MsqCtx *ctx);
void msq_row_begin  (MsqCtx *ctx);
void msq_row_end    (MsqCtx *ctx);
void msq_spacer     (MsqCtx *ctx, int px);
void msq_label      (MsqCtx *ctx, const char *text);
```

- **`msq_panel_begin(ctx, x, y, w)`** anchors a column at `(x, y)` with width
  `w`. Full-width widgets (slider, text input, textarea, list, column-mode
  dropdown) expand to fill `w`; buttons size to their label.
- **`msq_panel_end`** strokes a 1px border (`MSQ_COL_BORDER`) around everything
  emitted since `begin`. The panel *background* is whatever you cleared to.
- **`msq_row_begin` / `msq_row_end`** flow the enclosed widgets left-to-right;
  after `row_end` the cursor drops below the tallest widget in the row. Rows do
  **not** wrap — keep them narrower than the panel.
- **`msq_spacer(ctx, px)`** adds a gap: horizontal inside a row, vertical
  otherwise.
- **`msq_label(ctx, text)`** draws one line of static text and advances.

A [child window](#7-child-windows) sets up the same cursor for its body, so the
identical layout calls work inside a window.

---

## 6. Widgets

Every widget takes `ctx` first; most take a `label` that doubles as the
[identity seed](#8-widget-identity). "Returns `true`" always means *on that one
frame*.

### 6.1 Button

```c
bool msq_button(MsqCtx *ctx, const char *label);
```

Returns `true` on the frame the mouse is **released inside** the button after
being pressed inside it (a real click, not just mouse-down). Sizes to its label.

```c
if (msq_button(ui, "Save")) save_document();
```

### 6.2 Checkbox

```c
bool msq_checkbox(MsqCtx *ctx, const char *label, bool *value);
```

Toggles `*value` in place; returns `true` only on the frame it changed.

### 6.3 Slider

```c
bool msq_slider(MsqCtx *ctx, const char *label, float *value, float min, float max);
```

Horizontal slider over `[min, max]` editing `*value`; returns `true` while being
dragged. The track shows `"label: 0.42"`.

### 6.4 Text input

```c
bool msq_text_input(MsqCtx *ctx, const char *label, char *buf, int cap);
```

Single-line editable field over a **caller-owned** buffer; `cap` includes the
terminating NUL. Supports typing, Backspace, Left/Right caret movement, and a
blinking caret. Enter or clicking away commits (drops focus). If `label` is
non-empty it is drawn on its own line above the field. Returns `true` on any
frame the contents changed. Internally toggles `SDL_StartTextInput` /
`SDL_StopTextInput` on focus changes so IME and key-repeat work.

> Need selection or clipboard? Use the [textarea](#65-textarea) — the
> single-line input is intentionally minimal.

```c
char name[MSQ_TEXT_CAP] = "untitled";
if (msq_text_input(ui, "Name", name, MSQ_TEXT_CAP)) mark_dirty();
```

### 6.5 Textarea

```c
bool msq_textarea(MsqCtx *ctx, const char *label, char *buf, int cap, int visible_rows);
```

A multi-line editor over a caller-owned buffer (`cap` includes the NUL).
`visible_rows` sets the height in text rows; `≤ 0` picks a small default (5). If
`label` is non-empty it is drawn above. Returns `true` on frames the text
changed.

Capabilities:

- **Editing** — typing inserts at the caret; Enter inserts a newline;
  Backspace/Delete remove the selection or one character.
- **Selection** — click to place the caret, drag to select, or hold **Shift**
  with Left/Right/Home/End. The selection is highlighted, can span lines, and is
  replaced by typing or paste.
- **Navigation** — Left/Right, Home/End (line start/end), Up/Down (with column
  memory).
- **Clipboard** (system, via SDL) — **Ctrl+C** copy, **Ctrl+X** cut,
  **Ctrl+V** paste, **Ctrl+A** select-all.
- **Scrolling** — vertical, with a thin scrollbar; the view follows the caret
  when it moves and wheel-scrolls when hovered.

```c
char notes[2048] = "Type here.\nShift+arrows or mouse to select.";
msq_textarea(ui, "Notes", notes, sizeof notes, 8);
```

Limitations: no word-wrap (lines break only on `\n`; horizontal overflow is
clipped), and editing is byte-wise, so deleting in the middle of a multi-byte
UTF-8 character is imperfect.

### 6.6 List

```c
bool msq_list(MsqCtx *ctx, const char *label, const char *const *items,
              int count, int *selected, int visible_rows);
```

Single-selection listbox. `visible_rows` caps the visible height (`≤ 0` shows
all). When the content overflows it scrolls with the mouse wheel and shows a thin
3px scrollbar on the right. Edits `*selected`; returns `true` when the selection
changes. `label`, if non-empty, is drawn above.

```c
const char *fruits[] = { "Apple", "Banana", "Cherry", /* ... */ };
static int sel = 0;
if (msq_list(ui, "Fruit", fruits, N, &sel, 6)) on_pick(fruits[sel]);
```

### 6.7 Dropdown

```c
bool msq_dropdown(MsqCtx *ctx, const char *label, const char *const *items,
                  int count, int *selected);
```

A button showing the current choice that opens a popup of `items` beneath itself
— literally "button + popup menu." Edits `*selected` (use `-1` for "nothing
selected yet", which shows `Select...`). Returns `true` when the selection
changes. The open popup is drawn above other widgets via the deferred layer.

### 6.8 Popup menu

```c
MsqId msq_gen_id    (MsqCtx *ctx, const char *seed);
void  msq_popup_open(MsqCtx *ctx, MsqId id, int x, int y);
bool  msq_popup     (MsqCtx *ctx, MsqId id, const char *const *items,
                     int count, int *out_choice);
```

A free-floating menu you control:

1. Make a **stable** id once with `msq_gen_id(ctx, "some-seed")`.
2. Open it at a screen position (e.g. on right-click) with `msq_popup_open`.
3. Call `msq_popup` every frame to render and service it.

It dismisses on click-outside. When the user picks an item, the index is written
to `*out_choice` and the call returns `true` for that frame.

```c
MsqId menu = msq_gen_id(ui, "canvas-context");
const char *items[] = { "Cut", "Copy", "Paste", "Delete" };

// in event handling:
if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT)
    msq_popup_open(ui, menu, e.button.x, e.button.y);

// every frame:
int pick;
if (msq_popup(ui, menu, items, 4, &pick)) do_action(pick);
```

### 6.9 Tabs

```c
bool msq_tabs(MsqCtx *ctx, const char *label, const char *const *tabs,
              int count, int *selected);
```

A horizontal strip of tabs flowing left-to-right. The selected tab gets a
brighter background and an accent underline; others dim and highlight on hover.
Edits `*selected`; returns `true` when it changes. You switch your own content
underneath:

```c
static const char *pages[] = { "Info", "Style", "About" };
static int page = 0;
msq_tabs(ui, NULL, pages, 3, &page);
if      (page == 0) { /* info  widgets */ }
else if (page == 1) { /* style widgets */ }
else                { /* about widgets */ }
```

If `label` is non-empty it is drawn above the strip. Tabs don't wrap.

### 6.10 Tooltip

```c
void msq_tooltip(MsqCtx *ctx, const char *text);
```

Call **immediately after** the widget you want it on. When that widget has been
hovered for `MSQ_TOOLTIP_DELAY_MS` (default 500 ms) the tooltip is drawn near the
cursor during `msq_end_frame`.

```c
msq_button(ui, "Export");
msq_tooltip(ui, "render a PNG to disk");
```

---

## 7. Child windows

```c
bool msq_window_begin(MsqCtx *ctx, const char *title, int x, int y,
                      int w, int h, bool *open);
void msq_window_end  (MsqCtx *ctx);
```

Draggable, resizable, closable, auto-layering windows in the imgui / microui
style. Use the begin/end pattern, calling `msq_window_end` **only when**
`msq_window_begin` returned `true`:

```c
bool show = true;
if (msq_window_begin(ui, "Inspector", 40, 60, 300, 220, &show)) {
    msq_label(ui, "Transform");
    msq_slider(ui, "X", &x, -100, 100);
    msq_window_end(ui);
}
```

Behaviour:

- **Identity & persistence** — the window is keyed by `title` (a stable hash).
  Position, size, z-order, and open flag are remembered in the context across
  frames. Two live windows must have **distinct titles**.
- **Initial placement** — `(x, y, w, h)` is used only the first time that title
  is seen. Afterwards the user moves the window by dragging the **title bar** and
  resizes it by dragging the **bottom-right grip** (clamped to `MSQ_WIN_MIN_W` ×
  `MSQ_WIN_MIN_H`, default 90 × 56).
- **Closing** — pass a `bool *open` to get a working **X** button: clicking it
  sets `*open = false`. Set it back to `true` to reopen the window where it was.
  Pass `NULL` for a window with no close button.
- **Focus & layering** — the **top-most window under the cursor receives input**,
  so widgets in a covered window don't react to clicks through the window above.
  Clicking a window raises it to the front. Windows paint in z-order during
  `msq_end_frame`.
- **Clipping** — window content is clipped to the body interior, so widgets can
  never paint outside the window, even after it is resized smaller.
- **Fonts** — the title bar always uses the regular (TTF) font; body widgets
  follow the current [font setting](#9-fonts).

When `msq_window_begin` returns `false` (window closed or hidden), do **not**
call `msq_window_end`. The `if (...) { ...; msq_window_end(ui); }` shape handles
this automatically.

> Note: input gating is between windows. Top-level panel widgets are always live;
> if you place interactive top-level widgets *under* a window they can still
> react. Keep top-level widgets out from under your windows, or put everything in
> windows.

---

## 8. Widget identity

With no widget objects, mosquito derives a per-widget `MsqId` from an FNV-1a hash
of:

1. the widget's **label**,
2. the current **id-stack salt** (`msq_push_id`),
3. a **per-frame call counter** that ticks on each widget.

The call counter makes two same-labeled widgets distinct as long as they're
called in a consistent order — which covers most UIs for free.

```c
void  msq_push_id(MsqCtx *ctx, int salt);
void  msq_pop_id (MsqCtx *ctx);
MsqId msq_gen_id (MsqCtx *ctx, const char *seed);
```

- **`msq_push_id` / `msq_pop_id`** push/pop an integer salt mixed into every id
  while on the stack (depth up to `MSQ_ID_STACK`). Use it in **loops**, where
  call order alone can't keep ids stable as the data changes:

  ```c
  for (int i = 0; i < n; i++) {
      msq_push_id(ui, i);
      if (msq_button(ui, "Delete")) remove_item(i);
      msq_pop_id(ui);
  }
  ```

- **`msq_gen_id`** makes a **frame-stable** id from a string only (no call
  counter). Use it for popup ids that must persist across frames. Child-window
  titles are hashed this same stable way.

---

## 9. Fonts

mosquito ships **two** fonts and lets you mix them at runtime.

```c
typedef enum { MSQ_FONT_TTF, MSQ_FONT_RECT } MsqFontKind;

void msq_set_font      (MsqCtx *ctx, MsqFontKind kind);
void msq_set_rect_scale(MsqCtx *ctx, int scale);

void msq_rect_text  (SDL_Renderer *r, int x, int y, const char *s, int scale, MsqColor c);
int  msq_rect_text_w(const char *s, int scale);
int  msq_rect_text_h(int scale);
```

- **TTF** (`MSQ_FONT_TTF`, the default when a font loaded) — smooth UTF-8 text
  via SDL2_ttf.
- **Rect font** (`MSQ_FONT_RECT`) — a chunky **3×5 bitmap font**, always
  available even with SDL2_ttf compiled in. No blur, no allocation, perfect for
  tiny crisp captions. It covers ASCII **32–90** (space through `Z`); lowercase
  is drawn as uppercase, other glyphs are skipped. A glyph cell is `3*scale` wide
  and `5*scale` tall with a `scale`-pixel gap, so `msq_rect_text_w` is
  `len * 4 * scale` and `msq_rect_text_h` is `5 * scale`.

`msq_set_font` switches the font used by all widgets; `msq_set_rect_scale` sets
the rect-font pixel size (default 2, or 3 for `font_px ≥ 18`). The `msq_rect_text*`
helpers are standalone (no `MsqCtx`) and draw immediately on a renderer, matching
the other `draw_*` helpers.

If no TTF font is available (you passed `NULL`, the file failed to open, or you
built with `MSQ_NO_TTF`), widgets fall back to the rect font automatically rather
than drawing nothing.

```c
msq_set_font(ui, MSQ_FONT_RECT);     // crisp pixels everywhere
// ... or just for a one-off caption, regardless of the widget font:
msq_rect_text(ren, 10, 10, "FPS 60", 2, msq_color(MSQ_COL_TEXT_DIM));
```

---

## 10. Theming and colors

```c
typedef struct { uint8_t r, g, b, a; } MsqColor;

MsqColor msq_color    (MsqColorSlot slot);                 // default lookup
void     msq_set_color(MsqCtx *ctx, MsqColorSlot slot, MsqColor c);  // override
```

The palette is declared once as an X-macro table that expands into *both* the
`MsqColorSlot` enum and the default color array, so names and values can't drift
apart. Each entry is a packed `0xRRGGBBAA`:

```c
#define MSQ_THEME_COLORS(_)                    \
  _(MSQ_COL_WINDOW_BG,     0x1B1612FF)         \
  _(MSQ_COL_WIDGET_BG,     0x2E2620FF)         \
  _(MSQ_COL_WIDGET_HOT,    0x3B3129FF)         \
  _(MSQ_COL_WIDGET_ACTIVE, 0x534337FF)         \
  _(MSQ_COL_ACCENT,        0xB07A4CFF)         \
  _(MSQ_COL_ACCENT_DIM,    0x7E5A3BFF)         \
  _(MSQ_COL_TEXT,          0xECE2D8FF)         \
  _(MSQ_COL_TEXT_DIM,      0xA19588FF)         \
  _(MSQ_COL_BORDER,        0x120F0DFF)         \
  _(MSQ_COL_TOOLTIP_BG,    0x171310F0)         \
  _(MSQ_COL_POPUP_BG,      0x241E19FA)         \
  _(MSQ_COL_TITLEBAR,        0x2E2620FF)       \
  _(MSQ_COL_TITLEBAR_ACTIVE, 0x534337FF)       \
  _(MSQ_COL_CLOSE,           0x7E3B33FF)       \
  _(MSQ_COL_CLOSE_HOT,       0xB0463AFF)       \
  _(MSQ_COL_WINDOW_BODY,     0x231D18FF)
```

| Slot | Used for |
|------|----------|
| `MSQ_COL_WINDOW_BG` | The suggested screen-clear color. |
| `MSQ_COL_WIDGET_BG` / `_HOT` / `_ACTIVE` | Widget fill: idle / hovered / pressed. |
| `MSQ_COL_ACCENT` / `_DIM` | Selection, slider fill, knobs, tab underline. |
| `MSQ_COL_TEXT` / `_DIM` | Primary / secondary text. |
| `MSQ_COL_BORDER` | 1px outlines around widgets, panels, windows. |
| `MSQ_COL_TOOLTIP_BG` / `MSQ_COL_POPUP_BG` | Tooltip / popup backgrounds (slightly translucent). |
| `MSQ_COL_TITLEBAR` / `_ACTIVE` | Window title bar: unfocused / focused. |
| `MSQ_COL_CLOSE` / `_HOT` | Window close button: idle / hovered. |
| `MSQ_COL_WINDOW_BODY` | Window body fill. |

`msq_color` reads the compiled-in default; `msq_set_color` overrides a slot on a
live context. To re-skin everything without editing the source, `#define` your
own `MSQ_THEME_COLORS(...)` before the include that defines `MSQ_IMPLEMENTATION`.

---

## 11. Standalone drawing helpers

These take a bare `SDL_Renderer` (no `MsqCtx`) and are handy for custom widgets
or your own scene. Colors are packed `0xRRGGBBAA`; alpha blends because
`msq_create` enabled blend mode.

```c
void set_color_hex        (SDL_Renderer *r, uint32_t rgba);
void draw_shadow_simple   (SDL_Renderer *r, SDL_Rect rect);
void draw_frame           (SDL_Renderer *r, int x, int y, int w, int h,
                           int stroke, int color_a, int color_b);
void draw_v_arrow_rotated (SDL_Renderer *r, SDL_Rect rect, int padding,
                           int thickness, int rotation);   // 0=v 1=< 2=^ 3=>
void draw_triangle_rotated(SDL_Renderer *r, int ox, int oy, int size, int rotation);
void draw_triangle_angled (SDL_Renderer *r, int ox, int oy, int size, int degrees);
```

- `set_color_hex` — set the draw color from `0xRRGGBBAA`.
- `draw_shadow_simple` — a soft drop shadow (three stepped translucent rects).
- `draw_frame` — a beveled border: top/left use `color_a`, bottom/right
  `color_b`.
- `draw_v_arrow_rotated` — a chevron (used for dropdown/expander glyphs).
- `draw_triangle_rotated` / `draw_triangle_angled` — filled triangles; the
  angled variant snaps `degrees` to the nearest 90°.

Plus the rect-font helpers from [§9](#9-fonts): `msq_rect_text`,
`msq_rect_text_w`, `msq_rect_text_h`.

---

## 12. Compile-time configuration

Define these **before** the include that defines `MSQ_IMPLEMENTATION`:

| Macro | Default | Meaning |
|-------|---------|---------|
| `MSQ_TEXT_CAP` | `256` | Suggested capacity for user-owned text buffers. |
| `MSQ_MAX_POPUP_ITEMS` | `64` | Upper bound used when reasoning about popup sizes. |
| `MSQ_ID_STACK` | `32` | Maximum `msq_push_id` nesting depth. |
| `MSQ_MAX_WINDOWS` | `32` | Maximum distinct child windows. |
| `MSQ_MAX_DRAW_CMDS` | `2048` | Capacity of the per-frame window draw-command buffer. |
| `MSQ_DRAW_STR_ARENA` | `8192` | Bytes of per-frame string storage for recorded window text. |
| `MSQ_NO_TTF` | *unset* | Drop the SDL2_ttf dependency; use the rect font everywhere. |

If a window's content is unusually heavy (very long lists/textareas) and you hit
the command/arena caps, raise `MSQ_MAX_DRAW_CMDS` / `MSQ_DRAW_STR_ARENA`.

Internal tunables you can edit in the implementation block: `MSQ_TOOLTIP_DELAY_MS`
(500), `MSQ_PAD` (6), `MSQ_ROW_H` (26), `MSQ_GAP` (4), `MSQ_TITLEBAR_H` (22),
`MSQ_CLOSE_SIZE` (14), `MSQ_RESIZE_GRIP` (16), `MSQ_WIN_MIN_W` (90),
`MSQ_WIN_MIN_H` (56).

---

## 13. How it works inside

Most widgets draw immediately. **Child windows** are the exception, because
overlapping windows must paint and receive input in z-order even though you
declare them in arbitrary source order. mosquito handles this with a small
deferred command buffer:

1. Between `msq_window_begin` and `msq_window_end`, the low-level draw primitives
   (`fill`, `stroke`, `text`, `line`, `clip`, triangle, shadow) **record**
   commands into a per-frame buffer instead of drawing, tagged with the window's
   z-order. Recorded text is copied into a small string arena so transient
   strings stay valid until replay.
2. `msq_end_frame` walks the windows sorted by z (ascending) and **replays** each
   window's command slice, so the focused window paints last (on top). Then it
   draws popups and tooltips, which are deferred separately and land above the
   windows.
3. **Input routing**: at `msq_begin_frame`, mosquito finds the top-most window
   under the cursor (using last frame's geometry — a one-frame lag that's
   imperceptible). Only that window's body sees the real cursor; for every other
   window the cursor is moved far off-screen for the duration of its body, so its
   widgets can't react. The same trick disables body input while a window is
   being dragged or resized.
4. **Clipping**: each window installs a body clip rect. A list's or textarea's
   own clip is intersected with it, and clearing a clip restores the body clip
   rather than removing it — so content can never escape the window.

This is why two windows can overlap, be clicked to the front, and clip their
contents, all while staying a single-pass immediate-mode library.

---

## 14. Building and integrating

**Dependencies:** SDL2, and SDL2_ttf unless you build with `MSQ_NO_TTF`.

```sh
sudo apt install libsdl2-dev libsdl2-ttf-dev    # Debian/Ubuntu
```

**Use the provided script** to build the demos:

```sh
./build.sh          # SDL2_ttf (crisp text)  -> demo, demo_list, child_windows_demo
./build.sh nottf    # -DMSQ_NO_TTF (rect font only)
```

**Manual compile:**

```sh
gcc -Wall -Wextra -O2 yourapp.c -o yourapp $(sdl2-config --cflags --libs) -lSDL2_ttf -lm
# without SDL2_ttf:
gcc -Wall -Wextra -O2 -DMSQ_NO_TTF yourapp.c -o yourapp $(sdl2-config --cflags --libs) -lm
```

**Integrate into a project:**

1. Put `mosquito.h` on your include path.
2. In **exactly one** translation unit:
   ```c
   #define MSQ_IMPLEMENTATION
   #include "mosquito.h"
   ```
3. Elsewhere, `#include "mosquito.h"` without the define for the declarations.
4. Link `SDL2` (+ `SDL2_ttf` unless `MSQ_NO_TTF`) and `-lm`.

Because mosquito draws through your `SDL_Renderer`, declare your UI **after** your
scene each frame so it composites on top.

---

## 15. FAQ and gotchas

**My window never appears / `msq_window_end` crashes.**
Only call `msq_window_end` when `msq_window_begin` returned `true`. Use the
`if (msq_window_begin(...)) { ...; msq_window_end(ui); }` shape.

**Two windows behave as one.**
They share a title. Window identity is the title hash — give them distinct
titles (or wrap with `msq_push_id`).

**Buttons in a loop all act like the last one.**
Same-label widgets in a loop need a per-iteration salt: `msq_push_id(ui, i)` …
`msq_pop_id(ui)`.

**A top-level button under a window still clicks.**
Input gating is between windows; top-level widgets are always live. Don't place
interactive top-level widgets under a window, or move them into a window.

**Text is blank.**
You're on a code path with no font. Pass a valid `.ttf` to `msq_create`, or call
`msq_set_font(ui, MSQ_FONT_RECT)` to use the always-available bitmap font.

**The textarea garbles a non-ASCII character on delete.**
Editing is byte-wise; multi-byte UTF-8 mid-glyph deletion is imperfect by design.

**Right-click menu doesn't open.**
Call `msq_popup_open` from your event handling with a `msq_gen_id` id, and call
`msq_popup` with the same id every frame.

---

## 16. API cheat sheet

```c
/* lifecycle */
MsqCtx *msq_create (SDL_Renderer*, const char *ttf_path, int font_px);
void    msq_destroy(MsqCtx*);

/* frame */
void msq_begin_frame (MsqCtx*);
void msq_handle_event(MsqCtx*, const SDL_Event*);
void msq_end_frame   (MsqCtx*);

/* layout */
void msq_panel_begin(MsqCtx*, int x, int y, int w);
void msq_panel_end  (MsqCtx*);
void msq_row_begin  (MsqCtx*);
void msq_row_end    (MsqCtx*);
void msq_spacer     (MsqCtx*, int px);
void msq_label      (MsqCtx*, const char *text);

/* identity */
void  msq_push_id(MsqCtx*, int salt);
void  msq_pop_id (MsqCtx*);
MsqId msq_gen_id (MsqCtx*, const char *seed);

/* widgets */
bool msq_button    (MsqCtx*, const char *label);
bool msq_checkbox  (MsqCtx*, const char *label, bool *value);
bool msq_slider    (MsqCtx*, const char *label, float *value, float min, float max);
bool msq_text_input(MsqCtx*, const char *label, char *buf, int cap);
bool msq_textarea  (MsqCtx*, const char *label, char *buf, int cap, int visible_rows);
bool msq_list      (MsqCtx*, const char *label, const char *const *items,
                    int count, int *selected, int visible_rows);
bool msq_dropdown  (MsqCtx*, const char *label, const char *const *items,
                    int count, int *selected);
bool msq_tabs      (MsqCtx*, const char *label, const char *const *tabs,
                    int count, int *selected);
void msq_tooltip   (MsqCtx*, const char *text);

/* popups */
void msq_popup_open(MsqCtx*, MsqId id, int x, int y);
bool msq_popup     (MsqCtx*, MsqId id, const char *const *items, int count, int *out_choice);

/* child windows */
bool msq_window_begin(MsqCtx*, const char *title, int x, int y, int w, int h, bool *open);
void msq_window_end  (MsqCtx*);

/* fonts */
void msq_set_font      (MsqCtx*, MsqFontKind kind);
void msq_set_rect_scale(MsqCtx*, int scale);
void msq_rect_text     (SDL_Renderer*, int x, int y, const char *s, int scale, MsqColor c);
int  msq_rect_text_w   (const char *s, int scale);
int  msq_rect_text_h   (int scale);

/* theming */
MsqColor msq_color    (MsqColorSlot slot);
void     msq_set_color(MsqCtx*, MsqColorSlot slot, MsqColor c);
```

---

*0BSD / public-domain-equivalent. Do whatever you want.*
