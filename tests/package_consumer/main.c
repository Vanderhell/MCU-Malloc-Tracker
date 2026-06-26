#include <stdint.h>
#include <stdio.h>
#include "mt_wrap.h"

int main(void)
{
    mt_init();

    void* a = malloc(64);
    void* b = malloc(32);
    if (!a || !b) {
        return 1;
    }

    free(b);

    mt_heap_stats_t stats = mt_stats();
    if (stats.alloc_count != 1 || stats.current_used != 64) {
        return 1;
    }

    uint8_t snapshot[256];
    size_t written = mt_snapshot_write(snapshot, sizeof(snapshot));
    if (written < 36) {
        return 1;
    }

    free(a);

    return 0;
}
