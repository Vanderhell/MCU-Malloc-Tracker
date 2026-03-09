#ifndef MTV1_RENDER_TUI_H
#define MTV1_RENDER_TUI_H

#include "mtv1_model.h"
#include "mtv1_filemap.h"

typedef enum {
    TUI_SCREEN_OVERVIEW  = 1,
    TUI_SCREEN_TIMELINE  = 2,
    TUI_SCREEN_HEATMAP   = 3
} tui_screen_t;

typedef enum {
    HEATMAP_MODE_SIZE     = 0,
    HEATMAP_MODE_CALLSITE = 1
} heatmap_mode_t;

typedef struct {
    tui_screen_t   current_screen;
    heatmap_mode_t heatmap_mode;
    int            diff_mode;       /* 1 if two runs are loaded */
    mtv1_model_t*  run_a;
    mtv1_model_t*  run_b;           /* NULL if no diff */
    mtv1_filemap_t* filemap;        /* may be NULL */
    int            running;         /* 0 = quit */
} tui_state_t;

/* Platform init: enable VT processing on Windows, raw mode on POSIX */
int  tui_platform_init(void);
void tui_platform_cleanup(void);

/* Non-blocking key read: returns char or 0 if no key */
int  tui_read_key(void);

/* Clear screen and home cursor */
void tui_clear(void);

/* Render the current screen based on tui_state_t */
void tui_render(const tui_state_t* s);

/* Individual screen renderers (called by tui_render) */
void tui_render_overview(const tui_state_t* s);
void tui_render_timeline(const tui_state_t* s);
void tui_render_heatmap(const tui_state_t* s);
void tui_render_diff(const tui_state_t* s);

/* Main TUI event loop: blocks until quit */
void tui_run(tui_state_t* s);

/* Sparkline: renders timeline as 80-wide, 10-tall ASCII block graph.
   values[] is an array of uint32_t sample points.
   Outputs to stdout using block characters: ' ', '.', ':', '|', '#'. */
void tui_sparkline(const uint32_t* values, uint32_t count,
                   uint32_t* out_spikes, uint32_t* out_spike_count);

#endif /* MTV1_RENDER_TUI_H */
