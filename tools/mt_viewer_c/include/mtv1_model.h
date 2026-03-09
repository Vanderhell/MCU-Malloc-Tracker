#ifndef MTV1_MODEL_H
#define MTV1_MODEL_H

#include <stdint.h>
#include "mtv1_decode_mts1.h"

#define MTV1_MODEL_MAX_MARKS     256
#define MTV1_MODEL_MAX_TELEMETRY 256

/* One entry in the timeline array */
typedef struct {
    uint32_t frame_seq;
    uint32_t current_used;
    uint32_t peak_used;
    uint32_t total_allocs;
    uint32_t total_frees;
    uint32_t mts1_seq;
} mtv1_timeline_entry_t;

/* A mark message from a MARK_TEXT frame */
typedef struct {
    uint32_t frame_seq;
    char     text[128];
} mtv1_mark_t;

/* Hotspot aggregated across a run */
typedef struct {
    uint32_t file_id;
    uint16_t line;
    uint32_t total_allocs;
    uint32_t total_bytes;
    uint32_t last_snap_seq;
} mtv1_hotspot_t;

/* The full in-memory model of one loaded run */
typedef struct {
    /* Timeline: array of snapshot summaries */
    mtv1_timeline_entry_t* timeline;     /* malloc'd */
    uint32_t               timeline_len;
    uint32_t               timeline_cap;

    /* Last full snapshot (for Screen 1 detail) */
    mts1_snapshot_t*       last_snapshot; /* malloc'd, may be NULL */

    /* Top hotspots: aggregated from all snapshots */
    mtv1_hotspot_t*        hotspots;      /* malloc'd */
    uint32_t               hotspot_count;
    uint32_t               hotspot_cap;

    /* Marks */
    mtv1_mark_t            marks[MTV1_MODEL_MAX_MARKS];
    uint32_t               mark_count;

    /* Telemetry lines */
    char**                 telemetry;    /* malloc'd array of malloc'd strings */
    uint32_t               telemetry_count;
    uint32_t               telemetry_cap;

    /* Run-level stats */
    uint32_t               total_frames;
    uint32_t               resync_count;
    uint32_t               crc_fail_count;
    uint32_t               peak_used_ever;

    /* Source file path (for display) */
    char                   source_path[256];
} mtv1_model_t;

void         mtv1_model_init(mtv1_model_t* m);
void         mtv1_model_free(mtv1_model_t* m);

/* Ingest a parsed MTS1 snapshot into the model (updates timeline, hotspots) */
void         mtv1_model_push_snapshot(mtv1_model_t* m, uint32_t frame_seq,
                                       const mts1_snapshot_t* snap);

/* Ingest a TELEMETRY_TEXT or MARK_TEXT payload */
void         mtv1_model_push_telemetry(mtv1_model_t* m, uint32_t frame_seq,
                                        const char* text);
void         mtv1_model_push_mark(mtv1_model_t* m, uint32_t frame_seq,
                                   const char* text);

/* Get top N hotspots sorted by total_allocs DESC.
   out_buf must have capacity for n entries. Returns actual count written. */
uint32_t     mtv1_model_top_hotspots(const mtv1_model_t* m,
                                      mtv1_hotspot_t* out_buf, uint32_t n);

#endif /* MTV1_MODEL_H */
