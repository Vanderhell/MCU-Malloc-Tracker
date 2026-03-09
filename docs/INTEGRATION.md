# Integration Guide

## Overview

MCU Malloc Tracker can be integrated in multiple ways depending on your project structure and whether vendor libraries are involved.

---

## 1) Global Malloc Wrapping (Easiest for Debug Builds)

Use for projects where you control all allocations and want one-file integration:

```c
#include "mt_api.h"

// In your project header or main.c:
#define malloc(x)       mt_malloc((x), __FILE__, __LINE__)
#define free(p)         mt_free((p), __FILE__, __LINE__)
#define realloc(p, x)   mt_realloc((p), (x), __FILE__, __LINE__)

void main(void) {
    mt_init();
    // All malloc/free from now on are tracked
}
```

Or use the convenience header:

```c
#include "mt_wrap.h"  // Same defines, pre-packaged
```

---

## 2) Local (Per-Module) Wrapping

Use if vendor libraries or sensitive code must not be wrapped:

**Step 1**: Create a local header (e.g., in your own code):

```c
// my_allocators.h
#ifndef MY_ALLOCATORS_H
#define MY_ALLOCATORS_H

#include "mt_api.h"

#define malloc(x)       mt_malloc((x), __FILE__, __LINE__)
#define free(p)         mt_free((p), __FILE__, __LINE__)
#define realloc(p, x)   mt_realloc((p), (x), __FILE__, __LINE__)

#endif
```

**Step 2**: Include it only in your files, not vendor code:

```c
// my_driver.c
#include "my_allocators.h"
#include "my_driver.h"

void driver_init(void) {
    // malloc/free here are tracked
}

// vendor_lib.c (no #include "my_allocators.h")
void vendor_function(void) {
    // malloc/free here are NOT tracked (safe)
}
```

---

## 3) ISR Safety (Critical if malloc can be called from interrupt)

If your MCU calls `malloc/free/realloc` from interrupt service routines, you **must** define atomic protection:

```c
// In mt_config.h or before #include "mt_api.h":
#define MT_LOCK()   __disable_irq()
#define MT_UNLOCK() __enable_irq()
```

Or for FreeRTOS:

```c
#define MT_LOCK()   taskENTER_CRITICAL()
#define MT_UNLOCK() taskEXIT_CRITICAL()
```

Or for Zephyr:

```c
#define MT_LOCK()   k_sched_lock()
#define MT_UNLOCK() k_sched_unlock()
```

**Default** (if not defined): `MT_LOCK()` and `MT_UNLOCK()` are no-ops. Safe if malloc is only called from main code.

---

## 4) Libc Symbol Resolution

The tracker calls `MT_REAL_MALLOC`, `MT_REAL_FREE`, `MT_REAL_REALLOC` internally.

These must point to the actual libc functions, not your macros.

**If you wrap globally**, define them before:

```c
// Before mt_config.h or mt_api.h:
#define MT_REAL_MALLOC  malloc
#define MT_REAL_FREE    free
#define MT_REAL_REALLOC realloc

// Now define your macros (your malloc calls mt_malloc which calls MT_REAL_MALLOC)
#define malloc(x)       mt_malloc((x), __FILE__, __LINE__)
#define free(p)         mt_free((p), __FILE__, __LINE__)
#define realloc(p, x)   mt_realloc((p), (x), __FILE__, __LINE__)
```

**If you use the convenience header**, this is already handled:

```c
#include "mt_wrap.h"  // Defines both MT_REAL_* and malloc macros
```

---

## 5) Initialization

Call this once at system startup:

```c
void main(void) {
    mt_init();  // Safe to call multiple times (idempotent)
    // Rest of startup...
}
```

---

## 6) Getting Data Out

### Text dumps to UART

```c
static void uart_write(const char* s) {
    HAL_UART_Transmit(&huart1, (uint8_t*)s, strlen(s), HAL_MAX_DELAY);
}

// Later, when you want diagnostics:
mt_dump_uart(uart_write);     // All stats + table usage
mt_dump_leaks(uart_write);    // Active allocations (potential leaks)
mt_dump_hotspots(uart_write); // Where memory is allocated
```

### Binary snapshot for PC analysis

```c
uint8_t snapshot_buf[2048];
size_t snap_size = mt_snapshot_write(snapshot_buf, sizeof(snapshot_buf));

// Send snapshot_buf[0..snap_size] to PC (over UART, file, etc.)
// On PC: python3 tools/mt_decode.py snapshot.bin --filemap your_filemap.txt
```

### Getting stats programmatically

```c
mt_heap_stats_t stats = mt_stats();
printf("Current: %u bytes, Peak: %u bytes, Leaks: %u\n",
       stats.current_used, stats.peak_used, stats.alloc_count);
```

---

## 7) Build Integration

### CMake

```cmake
add_library(my_app
    src/main.c
    src/driver.c
    # Add tracker sources:
    ../mcu_malloc_tracker/src/mt_core.c
    ../mcu_malloc_tracker/src/mt_snapshot.c
    ../mcu_malloc_tracker/src/mt_dump.c
    ../mcu_malloc_tracker/src/mt_hotspots.c
    ../mcu_malloc_tracker/src/mt_heap_stats.c
    ../mcu_malloc_tracker/src/mt_fragmentation.c
    ../mcu_malloc_tracker/src/mt_crc32_ieee.c
)

target_include_directories(my_app PRIVATE
    ../mcu_malloc_tracker/include
)

target_compile_definitions(my_app PRIVATE
    MT_MAX_ALLOCS=512
    MT_ENABLE_SNAPSHOT=1
    MT_ENABLE_HOTSPOTS=1
)
```

### Makefile

```makefile
TRACKER_DIR := ../mcu_malloc_tracker
TRACKER_SRC := $(TRACKER_DIR)/src/mt_core.c \
               $(TRACKER_DIR)/src/mt_snapshot.c \
               $(TRACKER_DIR)/src/mt_dump.c \
               $(TRACKER_DIR)/src/mt_hotspots.c \
               $(TRACKER_DIR)/src/mt_heap_stats.c \
               $(TRACKER_DIR)/src/mt_fragmentation.c \
               $(TRACKER_DIR)/src/mt_crc32_ieee.c

CFLAGS += -I$(TRACKER_DIR)/include \
          -DMT_MAX_ALLOCS=512 \
          -DMT_ENABLE_SNAPSHOT=1 \
          -DMT_ENABLE_HOTSPOTS=1

OBJS += $(TRACKER_SRC:.c=.o)
```

### STM32CubeIDE / ESP-IDF

See project-specific integration guides in `docs/`.

---

## 8) Testing Your Integration

Quick test after integration:

```c
#include "mt_api.h"

void test_malloc_tracker(void) {
    mt_init();

    void* p1 = malloc(64);
    void* p2 = malloc(128);
    free(p1);
    void* p3 = malloc(256);

    mt_heap_stats_t stats = mt_stats();
    assert(stats.total_allocs == 3);
    assert(stats.total_frees == 1);
    assert(stats.alloc_count == 2);

    printf("Tracker is working!\n");
}
```

---

## Common Issues

| Issue | Solution |
|-------|----------|
| Undefined `MT_REAL_MALLOC` | Include `mt_wrap.h` or define it yourself before macros |
| "Malloc called recursively" | MT_LOCK/MT_UNLOCK not defined and malloc called from interrupt |
| "table_drops > 0" | Increase `MT_MAX_ALLOCS` (must be power-of-two) |
| Filenames show as hashes, not names | Provide `--filemap` file to `mt_decode.py` (see FILEMAP.md) |

---

## Next Steps

- Read `docs/KNOWN_LIMITS.md` for limitations
- Read `docs/CONFIG.md` for all configuration options
- See examples in `examples/` directory
- Decode snapshot dumps with `tools/mt_decode.py`
