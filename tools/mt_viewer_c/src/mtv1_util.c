#include "mtv1_util.h"
#include <string.h>
#include <stdio.h>

/* ===== Little-endian readers ===== */

uint16_t util_read_u16le(const uint8_t* p) {
    return ((uint16_t)p[0]) | (((uint16_t)p[1]) << 8);
}

uint32_t util_read_u32le(const uint8_t* p) {
    return ((uint32_t)p[0])
         | (((uint32_t)p[1]) << 8)
         | (((uint32_t)p[2]) << 16)
         | (((uint32_t)p[3]) << 24);
}

uint64_t util_read_u64le(const uint8_t* p) {
    return ((uint64_t)p[0])
         | (((uint64_t)p[1]) << 8)
         | (((uint64_t)p[2]) << 16)
         | (((uint64_t)p[3]) << 24)
         | (((uint64_t)p[4]) << 32)
         | (((uint64_t)p[5]) << 40)
         | (((uint64_t)p[6]) << 48)
         | (((uint64_t)p[7]) << 56);
}

/* ===== Argument parser ===== */

void mtv1_args_init(mtv1_args_t* a, int argc, char** argv) {
    a->argc = argc;
    a->argv = argv;
    a->pos = 1;
}

const char* mtv1_args_get(mtv1_args_t* a, const char* flag) {
    for (int i = 1; i < a->argc - 1; i++) {
        if (strcmp(a->argv[i], flag) == 0)
            return a->argv[i + 1];
    }
    return NULL;
}

int mtv1_args_has(mtv1_args_t* a, const char* flag) {
    for (int i = 1; i < a->argc; i++) {
        if (strcmp(a->argv[i], flag) == 0)
            return 1;
    }
    return 0;
}

const char* mtv1_args_positional(mtv1_args_t* a, int index) {
    if (index + 1 < a->argc)
        return a->argv[index + 1];
    return NULL;
}

/* ===== String helpers ===== */

void util_str_copy(char* dst, size_t dst_cap, const char* src) {
    if (dst_cap == 0) return;
    size_t i = 0;
    while (i < dst_cap - 1 && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

int util_str_starts(const char* s, const char* prefix) {
    while (*prefix) {
        if (*s != *prefix) return 0;
        s++;
        prefix++;
    }
    return 1;
}

/* ===== Sorting ===== */

void util_sort_u32_desc(uint32_t* arr, uint32_t count) {
    /* Simple O(n²) insertion sort, descending */
    for (uint32_t i = 1; i < count; i++) {
        uint32_t key = arr[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && arr[j] < key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

/* ===== Formatting ===== */

void util_format_bytes(uint32_t bytes, char* out, size_t cap) {
    if (bytes < 1024) {
        snprintf(out, cap, "%u B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(out, cap, "%.1f KB", (double)bytes / 1024.0);
    } else {
        snprintf(out, cap, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
    }
}
