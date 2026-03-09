#ifndef MTV1_UTIL_H
#define MTV1_UTIL_H

#include <stdint.h>
#include <stddef.h>

/* Little-endian readers */
uint16_t util_read_u16le(const uint8_t* p);
uint32_t util_read_u32le(const uint8_t* p);
uint64_t util_read_u64le(const uint8_t* p);

/* Simple arg parser (no getopt on MSVC) */
typedef struct {
    int    argc;
    char** argv;
    int    pos;
} mtv1_args_t;

void        mtv1_args_init(mtv1_args_t* a, int argc, char** argv);
const char* mtv1_args_get(mtv1_args_t* a, const char* flag);
int         mtv1_args_has(mtv1_args_t* a, const char* flag);
const char* mtv1_args_positional(mtv1_args_t* a, int index);

/* String helpers */
void util_str_copy(char* dst, size_t dst_cap, const char* src);
int  util_str_starts(const char* s, const char* prefix);

/* Sorting: O(n²) insertion sort, descending */
void util_sort_u32_desc(uint32_t* arr, uint32_t count);

/* Formatting */
void util_format_bytes(uint32_t bytes, char* out, size_t cap);

#endif /* MTV1_UTIL_H */
