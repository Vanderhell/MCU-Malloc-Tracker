#include <stdint.h>
#include <stdio.h>
#include "../include/mt_api.h"
#include "wrapper_multitu.h"

static int check_stats(uint32_t alloc_count, uint32_t current_used)
{
    mt_heap_stats_t stats = mt_stats();
    return stats.alloc_count == alloc_count && stats.current_used == current_used;
}

int main(void)
{
    mt_init();

    void* wrapped = mt_test_wrapped_alloc(32);
    void* plain = mt_test_plain_alloc(48);

    if (!wrapped || !plain) {
        return 1;
    }

    if (!check_stats(1, 32)) {
        return 1;
    }

    mt_test_plain_free(plain);
    if (!check_stats(1, 32)) {
        return 1;
    }

    mt_test_wrapped_free(wrapped);
    if (!check_stats(0, 0)) {
        return 1;
    }

    return 0;
}
