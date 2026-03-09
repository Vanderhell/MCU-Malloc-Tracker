#include "mtv1_filemap.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void mtv1_filemap_init(mtv1_filemap_t* fm) {
    memset(fm, 0, sizeof(*fm));
    fm->cap = 64;
    fm->entries = (mtv1_filemap_entry_t*)malloc(fm->cap * sizeof(mtv1_filemap_entry_t));
}

void mtv1_filemap_free(mtv1_filemap_t* fm) {
    if (!fm) return;
    if (fm->entries) {
        free(fm->entries);
        fm->entries = NULL;
    }
    fm->count = 0;
    fm->cap = 0;
}

int mtv1_filemap_load(mtv1_filemap_t* fm, const char* path) {
    if (!fm || !path) return -1;

    FILE* f = fopen(path, "r");
    if (!f) return -1;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        /* Parse: "0xHASH path/to/file" */
        uint32_t file_id = 0;
        char filepath[128] = "";

        int n = sscanf(line, "%x %127s", &file_id, filepath);
        if (n != 2) continue;

        /* Grow if needed */
        if (fm->count >= fm->cap) {
            fm->cap *= 2;
            fm->entries = (mtv1_filemap_entry_t*)realloc(fm->entries,
                                                         fm->cap * sizeof(mtv1_filemap_entry_t));
        }

        mtv1_filemap_entry_t* entry = &fm->entries[fm->count++];
        entry->file_id = file_id;
        strncpy(entry->path, filepath, sizeof(entry->path) - 1);
        entry->path[sizeof(entry->path) - 1] = '\0';
    }

    fclose(f);
    return 0;
}

const char* mtv1_filemap_lookup(const mtv1_filemap_t* fm, uint32_t file_id) {
    if (!fm) return NULL;

    for (uint32_t i = 0; i < fm->count; i++) {
        if (fm->entries[i].file_id == file_id)
            return fm->entries[i].path;
    }
    return NULL;
}

void mtv1_filemap_format(const mtv1_filemap_t* fm, uint32_t file_id,
                          uint16_t line, char* out_buf, size_t out_cap) {
    if (!out_buf || out_cap == 0) return;

    const char* path = NULL;
    if (fm)
        path = mtv1_filemap_lookup(fm, file_id);

    if (path) {
        snprintf(out_buf, out_cap, "%s:%u", path, line);
    } else {
        snprintf(out_buf, out_cap, "0x%X:%u", file_id, line);
    }
}
