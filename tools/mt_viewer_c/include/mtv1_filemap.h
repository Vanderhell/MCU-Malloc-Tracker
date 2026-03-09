#ifndef MTV1_FILEMAP_H
#define MTV1_FILEMAP_H

#include <stdint.h>

#define MTV1_FILEMAP_MAX_ENTRIES 512

typedef struct {
    uint32_t file_id;
    char     path[128];
} mtv1_filemap_entry_t;

typedef struct {
    mtv1_filemap_entry_t* entries;  /* malloc'd */
    uint32_t              count;
    uint32_t              cap;
} mtv1_filemap_t;

void         mtv1_filemap_init(mtv1_filemap_t* fm);
void         mtv1_filemap_free(mtv1_filemap_t* fm);

/* Load from text file: "0xHASH path/to/file.c\n" format */
int          mtv1_filemap_load(mtv1_filemap_t* fm, const char* path);

/* Lookup: returns path string or NULL */
const char*  mtv1_filemap_lookup(const mtv1_filemap_t* fm, uint32_t file_id);

/* Format file_id:line as "path:line" or "0xHASH:line" if not found.
   out_buf must be at least 128+16 bytes. */
void         mtv1_filemap_format(const mtv1_filemap_t* fm, uint32_t file_id,
                                  uint16_t line, char* out_buf, size_t out_cap);

#endif /* MTV1_FILEMAP_H */
