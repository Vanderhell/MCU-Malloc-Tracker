#include "mtv1_model.h"
#include <stdlib.h>
#include <string.h>

void mtv1_model_init(mtv1_model_t* m) {
    memset(m, 0, sizeof(*m));
    m->timeline_cap = 64;
    m->timeline = (mtv1_timeline_entry_t*)malloc(m->timeline_cap * sizeof(mtv1_timeline_entry_t));
    m->hotspot_cap = 64;
    m->hotspots = (mtv1_hotspot_t*)malloc(m->hotspot_cap * sizeof(mtv1_hotspot_t));
    m->telemetry_cap = 32;
    m->telemetry = (char**)malloc(m->telemetry_cap * sizeof(char*));
}

void mtv1_model_free(mtv1_model_t* m) {
    if (!m) return;

    if (m->timeline) {
        free(m->timeline);
        m->timeline = NULL;
    }
    if (m->hotspots) {
        free(m->hotspots);
        m->hotspots = NULL;
    }
    if (m->last_snapshot) {
        mts1_snapshot_free(m->last_snapshot);
        free(m->last_snapshot);
        m->last_snapshot = NULL;
    }
    if (m->telemetry) {
        for (uint32_t i = 0; i < m->telemetry_count; i++) {
            if (m->telemetry[i])
                free(m->telemetry[i]);
        }
        free(m->telemetry);
        m->telemetry = NULL;
    }
}

void mtv1_model_push_snapshot(mtv1_model_t* m, uint32_t frame_seq,
                               const mts1_snapshot_t* snap) {
    if (!m) return;

    /* Grow timeline if needed */
    if (m->timeline_len >= m->timeline_cap) {
        m->timeline_cap *= 2;
        m->timeline = (mtv1_timeline_entry_t*)realloc(m->timeline,
                                                      m->timeline_cap * sizeof(mtv1_timeline_entry_t));
    }

    /* Add timeline entry */
    mtv1_timeline_entry_t* entry = &m->timeline[m->timeline_len++];
    entry->frame_seq = frame_seq;
    entry->current_used = snap->hdr.current_used;
    entry->peak_used = snap->hdr.peak_used;
    entry->total_allocs = snap->hdr.total_allocs;
    entry->total_frees = snap->hdr.total_frees;
    entry->mts1_seq = snap->hdr.seq;

    /* Update run stats */
    if (snap->hdr.peak_used > m->peak_used_ever)
        m->peak_used_ever = snap->hdr.peak_used;

    /* Update last snapshot */
    if (m->last_snapshot) {
        mts1_snapshot_free(m->last_snapshot);
        free(m->last_snapshot);
    }
    m->last_snapshot = (mts1_snapshot_t*)malloc(sizeof(mts1_snapshot_t));
    memcpy(m->last_snapshot, snap, sizeof(mts1_snapshot_t));

    /* Deep copy records */
    if (snap->hdr.record_count > 0) {
        m->last_snapshot->records = (mts1_record_t*)malloc(
            snap->hdr.record_count * sizeof(mts1_record_t));
        memcpy(m->last_snapshot->records, snap->records,
               snap->hdr.record_count * sizeof(mts1_record_t));
    } else {
        m->last_snapshot->records = NULL;
    }

    /* Aggregate hotspots from this snapshot */
    for (uint32_t i = 0; i < snap->hdr.record_count; i++) {
        const mts1_record_t* rec = &snap->records[i];

        /* Find or create hotspot */
        mtv1_hotspot_t* hs = NULL;
        for (uint32_t j = 0; j < m->hotspot_count; j++) {
            if (m->hotspots[j].file_id == rec->file_id &&
                m->hotspots[j].line == rec->line) {
                hs = &m->hotspots[j];
                break;
            }
        }

        if (!hs) {
            /* New hotspot */
            if (m->hotspot_count >= m->hotspot_cap) {
                m->hotspot_cap *= 2;
                m->hotspots = (mtv1_hotspot_t*)realloc(m->hotspots,
                                                       m->hotspot_cap * sizeof(mtv1_hotspot_t));
            }
            hs = &m->hotspots[m->hotspot_count++];
            hs->file_id = rec->file_id;
            hs->line = rec->line;
            hs->total_allocs = 0;
            hs->total_bytes = 0;
        }

        hs->total_allocs++;
        hs->total_bytes += rec->size;
        hs->last_snap_seq = snap->hdr.seq;
    }

    m->total_frames++;
}

void mtv1_model_push_telemetry(mtv1_model_t* m, uint32_t frame_seq,
                                const char* text) {
    (void)frame_seq; /* unused for now */
    if (!m || !text) return;

    if (m->telemetry_count >= m->telemetry_cap) {
        m->telemetry_cap *= 2;
        m->telemetry = (char**)realloc(m->telemetry,
                                       m->telemetry_cap * sizeof(char*));
    }

    size_t len = strlen(text);
    char* copy = (char*)malloc(len + 1);
    strcpy(copy, text);
    m->telemetry[m->telemetry_count++] = copy;
}

void mtv1_model_push_mark(mtv1_model_t* m, uint32_t frame_seq,
                           const char* text) {
    if (!m || !text || m->mark_count >= MTV1_MODEL_MAX_MARKS) return;

    mtv1_mark_t* mark = &m->marks[m->mark_count++];
    mark->frame_seq = frame_seq;
    strncpy(mark->text, text, sizeof(mark->text) - 1);
    mark->text[sizeof(mark->text) - 1] = '\0';
}

uint32_t mtv1_model_top_hotspots(const mtv1_model_t* m,
                                  mtv1_hotspot_t* out_buf, uint32_t n) {
    if (!m || !out_buf || n == 0) return 0;

    uint32_t count = m->hotspot_count < n ? m->hotspot_count : n;

    /* Copy to output buffer */
    for (uint32_t i = 0; i < count; i++)
        out_buf[i] = m->hotspots[i];

    /* Simple O(n²) insertion sort by total_allocs DESC */
    for (uint32_t i = 1; i < count; i++) {
        mtv1_hotspot_t key = out_buf[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && out_buf[j].total_allocs < key.total_allocs) {
            out_buf[j + 1] = out_buf[j];
            j--;
        }
        out_buf[j + 1] = key;
    }

    return count;
}
