#include "mtv1_render_tui.h"
#include "mtv1_platform.h"
#include "mtv1_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if MTV1_PLATFORM_WIN32
#include <windows.h>
#include <conio.h>

int tui_platform_init(void) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(h, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(h, mode) ? 0 : -1;
}

void tui_platform_cleanup(void) {
    /* No cleanup needed */
}

static int tui_read_key(void) {
    return _kbhit() ? _getch() : 0;
}

static void platform_sleep_ms(int ms) {
    Sleep(ms);
}

#else
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

static struct termios g_orig_tios;

int tui_platform_init(void) {
    tcgetattr(STDIN_FILENO, &g_orig_tios);
    struct termios raw = g_orig_tios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    return tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

void tui_platform_cleanup(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_tios);
}

static int tui_read_key(void) {
    char c = 0;
    int n = read(STDIN_FILENO, &c, 1);
    return (n > 0) ? (unsigned char)c : 0;
}

static void platform_sleep_ms(int ms) {
    usleep(ms * 1000);
}
#endif

/* ===== TUI Utilities ===== */

void tui_clear(void) {
    printf("\033[2J\033[H");
    fflush(stdout);
}

void tui_set_bold(void) {
    printf("\033[1m");
}

void tui_set_reverse(void) {
    printf("\033[7m");
}

void tui_reset_attrib(void) {
    printf("\033[0m");
}

/* ===== Screen Renderers ===== */

void tui_render_overview(const tui_state_t* s) {
    tui_set_reverse();
    printf("=== MTV1 Viewer [Screen 1/3: Overview] ===");
    tui_reset_attrib();
    printf("  Keys: 1/2/3 h d q\n\n");

    if (s->run_a) {
        printf("Run: %s   Frames: %u   Resyncs: %u   CRC Fails: %u\n\n",
               s->run_a->source_path, s->run_a->total_frames,
               s->run_a->resync_count, s->run_a->crc_fail_count);

        if (s->run_a->last_snapshot) {
            const mts1_header_t* hdr = &s->run_a->last_snapshot->hdr;
            printf("--- Last Snapshot (seq=%u) ---\n", hdr->seq);
            printf("  Current Used : %u B\n", hdr->current_used);
            printf("  Peak Used    : %u B\n", hdr->peak_used);
            printf("  Total Allocs : %u\n", hdr->total_allocs);
            printf("  Total Frees  : %u\n", hdr->total_frees);
            printf("  Active Allocs: %u\n\n", hdr->record_count);
        }

        printf("--- Top 10 Hotspots ---\n");
        mtv1_hotspot_t top[10];
        uint32_t count = mtv1_model_top_hotspots(s->run_a, top, 10);
        printf("  Rank  FileID       Line   Allocs  Bytes\n");
        for (uint32_t i = 0; i < count; i++) {
            char formatted[144];
            if (s->filemap)
                mtv1_filemap_format(s->filemap, top[i].file_id, top[i].line, formatted, sizeof(formatted));
            else
                snprintf(formatted, sizeof(formatted), "0x%X:%u", top[i].file_id, top[i].line);

            printf("  %3u.  %-32s  %6u  %6u\n",
                   i+1, formatted, top[i].total_allocs, top[i].total_bytes);
        }
    }
}

void tui_render_timeline(const tui_state_t* s) {
    tui_set_reverse();
    printf("=== MTV1 Viewer [Screen 2/3: Timeline] ===");
    tui_reset_attrib();
    printf("\n\n");

    if (!s->run_a || s->run_a->timeline_len < 2) {
        printf("Not enough timeline data\n");
        return;
    }

    const mtv1_timeline_entry_t* timeline = s->run_a->timeline;
    uint32_t count = s->run_a->timeline_len;

    /* Find peak and avg */
    uint32_t peak = 0, min_val = UINT32_MAX;
    uint64_t sum = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (timeline[i].current_used > peak)
            peak = timeline[i].current_used;
        if (timeline[i].current_used < min_val)
            min_val = timeline[i].current_used;
        sum += timeline[i].current_used;
    }
    uint32_t avg = sum / count;

    printf("Current: %u B   Peak: %u B   Avg: %u B\n\n", timeline[count-1].current_used, peak, avg);

    /* Draw sparkline: 80 wide, 10 tall */
    #define SPARKLINE_WIDTH 80
    #define SPARKLINE_HEIGHT 10
    char grid[SPARKLINE_HEIGHT][SPARKLINE_WIDTH];

    memset(grid, ' ', sizeof(grid));

    for (int col = 0; col < SPARKLINE_WIDTH; col++) {
        uint32_t idx = (col * count) / SPARKLINE_WIDTH;
        if (idx >= count) idx = count - 1;

        uint32_t val = timeline[idx].current_used;
        int bar_height = (val > 0 && peak > 0) ? ((val * SPARKLINE_HEIGHT) / peak) : 0;
        if (bar_height > SPARKLINE_HEIGHT) bar_height = SPARKLINE_HEIGHT;

        for (int row = 0; row < bar_height; row++)
            grid[SPARKLINE_HEIGHT - 1 - row][col] = '#';
    }

    /* Print grid with Y-axis labels */
    for (int row = 0; row < SPARKLINE_HEIGHT; row++) {
        if (row == 0)
            printf("%6u |", peak);
        else if (row == SPARKLINE_HEIGHT - 1)
            printf("     0 |");
        else
            printf("      |");

        for (int col = 0; col < SPARKLINE_WIDTH; col++)
            putchar(grid[row][col]);
        printf("\n");
    }

    printf("      +");
    for (int i = 0; i < SPARKLINE_WIDTH; i++)
        putchar('-');
    printf("> frame\n");
    #undef SPARKLINE_WIDTH
    #undef SPARKLINE_HEIGHT
}

void tui_sparkline(const uint32_t* values, uint32_t count,
                   uint32_t* out_spikes, uint32_t* out_spike_count) {
    (void)values;
    (void)count;
    (void)out_spikes;
    (void)out_spike_count;
}

void tui_render_heatmap(const tui_state_t* s) {
    tui_set_reverse();
    printf("=== MTV1 Viewer [Screen 3/3: Heatmap] ===");
    tui_reset_attrib();
    if (s->heatmap_mode == HEATMAP_MODE_SIZE)
        printf(" (Size Buckets, h=toggle)\n\n");
    else
        printf(" (Callsite Hotspots, h=toggle)\n\n");

    if (!s->run_a || !s->run_a->last_snapshot) {
        printf("No snapshot data\n");
        return;
    }

    if (s->heatmap_mode == HEATMAP_MODE_SIZE) {
        /* Size buckets */
        const char* buckets[] = {
            "[   0-15]",
            "[  16-31]",
            "[  32-63]",
            "[ 64-127]",
            "[128-255]",
            "[256-511]",
            "[512-1023]",
            "[1024+]"
        };
        uint32_t bucket_allocs[8] = {0};
        uint32_t bucket_bytes[8] = {0};

        for (uint32_t i = 0; i < s->run_a->last_snapshot->hdr.record_count; i++) {
            uint32_t size = s->run_a->last_snapshot->records[i].size;
            int bucket = 7;
            if (size < 16) bucket = 0;
            else if (size < 32) bucket = 1;
            else if (size < 64) bucket = 2;
            else if (size < 128) bucket = 3;
            else if (size < 256) bucket = 4;
            else if (size < 512) bucket = 5;
            else if (size < 1024) bucket = 6;

            bucket_allocs[bucket]++;
            bucket_bytes[bucket] += size;
        }

        uint32_t max_allocs = 0;
        for (int i = 0; i < 8; i++)
            if (bucket_allocs[i] > max_allocs)
                max_allocs = bucket_allocs[i];

        for (int i = 0; i < 8; i++) {
            printf("  %s ", buckets[i]);
            int bar_len = (max_allocs > 0) ? ((bucket_allocs[i] * 30) / max_allocs) : 0;
            for (int j = 0; j < bar_len; j++)
                putchar('#');
            printf(" %3u allocs %6u B\n", bucket_allocs[i], bucket_bytes[i]);
        }
    } else {
        /* Callsite hotspots */
        mtv1_hotspot_t top[16];
        uint32_t count = mtv1_model_top_hotspots(s->run_a, top, 16);

        uint32_t max_allocs = 0;
        for (uint32_t i = 0; i < count; i++)
            if (top[i].total_allocs > max_allocs)
                max_allocs = top[i].total_allocs;

        for (uint32_t i = 0; i < count; i++) {
            char formatted[144];
            if (s->filemap)
                mtv1_filemap_format(s->filemap, top[i].file_id, top[i].line, formatted, sizeof(formatted));
            else
                snprintf(formatted, sizeof(formatted), "0x%X:%u", top[i].file_id, top[i].line);

            printf("  %-40s ", formatted);
            int bar_len = (max_allocs > 0) ? ((top[i].total_allocs * 30) / max_allocs) : 0;
            for (int j = 0; j < bar_len; j++)
                putchar('#');
            printf(" %6u allocs\n", top[i].total_allocs);
        }
    }
}

void tui_render_diff(const tui_state_t* s) {
    tui_set_reverse();
    printf("=== MTV1 Viewer [Screen D: Diff View] ===\n");
    tui_reset_attrib();

    if (!s->run_a || !s->run_b) {
        printf("No diff data\n");
        return;
    }

    printf("Run A vs Run B\n\n");

    int32_t peak_delta = (int32_t)s->run_b->peak_used_ever - (int32_t)s->run_a->peak_used_ever;
    printf("Peak Delta: %+d B %s\n", peak_delta, peak_delta > 0 ? "(worse)" : "(better)");
    printf("Snapshots: %u vs %u\n", s->run_a->timeline_len, s->run_b->timeline_len);
    printf("Hotspots: %u vs %u\n", s->run_a->hotspot_count, s->run_b->hotspot_count);
}

void tui_render(const tui_state_t* s) {
    if (!s) return;

    switch (s->current_screen) {
        case TUI_SCREEN_OVERVIEW:
            tui_render_overview(s);
            break;
        case TUI_SCREEN_TIMELINE:
            tui_render_timeline(s);
            break;
        case TUI_SCREEN_HEATMAP:
            tui_render_heatmap(s);
            break;
        default:
            break;
    }

    printf("\n[q]uit [1/2/3]screens [h]eatmap [d]iff\n");
}

void tui_run(tui_state_t* s) {
    if (!s) return;

    tui_clear();
    tui_render(s);

    while (s->running) {
        int key = tui_read_key();

        if (key == 'q' || key == 'Q') {
            s->running = 0;
        } else if (key == '1') {
            s->current_screen = TUI_SCREEN_OVERVIEW;
        } else if (key == '2') {
            s->current_screen = TUI_SCREEN_TIMELINE;
        } else if (key == '3') {
            s->current_screen = TUI_SCREEN_HEATMAP;
        } else if (key == 'h' || key == 'H') {
            s->heatmap_mode = (s->heatmap_mode == HEATMAP_MODE_SIZE)
                            ? HEATMAP_MODE_CALLSITE
                            : HEATMAP_MODE_SIZE;
        } else if (key == 'd' || key == 'D') {
            if (s->run_b)
                tui_render_diff(s);
        }

        if (key) {
            tui_clear();
            tui_render(s);
        }

        platform_sleep_ms(100);
    }

    tui_clear();
    printf("Exiting MTV1 viewer\n");
}
