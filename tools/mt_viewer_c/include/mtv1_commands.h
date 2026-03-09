#ifndef MTV1_COMMANDS_H
#define MTV1_COMMANDS_H

#include <stdint.h>
#include "mtv1_model.h"

/* Capture command options */
typedef struct {
    const char*        port;
    uint32_t           baud;
    const char*        out_path;
    int                seconds;
} mtv1_capture_opts_t;

int mtv1_capture_run(const mtv1_capture_opts_t* opts);

/* Replay command options */
typedef struct {
    const char*    in_path;
    const char*    filemap_path;
    int            no_tui;
} mtv1_replay_opts_t;

int mtv1_replay_run(const mtv1_replay_opts_t* opts);

/* Diff command options */
typedef struct {
    const char* path_a;
    const char* path_b;
    const char* out_path;
    const char* filemap_path;
} mtv1_diff_opts_t;

typedef struct {
    int32_t  peak_delta;
    double   slope_delta;
    uint32_t spike_count;
    uint32_t new_hotspot_count;
    uint32_t resolved_hotspot_count;
} mtv1_diff_result_t;

int mtv1_diff_run(const mtv1_diff_opts_t* opts);

int mtv1_diff_compute(const mtv1_model_t* a, const mtv1_model_t* b,
                      mtv1_diff_result_t* out);

/* Export command options */
typedef struct {
    const char* in_path;
    const char* csv_path;
    const char* jsonl_path;
    const char* filemap_path;
} mtv1_export_opts_t;

int mtv1_export_run(const mtv1_export_opts_t* opts);

#endif /* MTV1_COMMANDS_H */
