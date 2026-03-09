/*
 * Helper: Generate snapshot dump file for decoder testing
 */

#include <stdio.h>
#include <stdlib.h>
#include "../../include/mt_config.h"
#include "../../include/mt_api.h"

#define malloc(x)       mt_malloc((x), __FILE__, __LINE__)
#define free(p)         mt_free((p), __FILE__, __LINE__)
#define realloc(p, x)   mt_realloc((p), (x), __FILE__, __LINE__)

int main(void)
{
    uint8_t snapshot_buf[2048];

    mt_init();

    void* b1 = malloc(64);
    void* b2 = malloc(128);
    void* b3 = malloc(256);

    free(b1);
    realloc(b3, 512);

    size_t snap_size = mt_snapshot_write(snapshot_buf, sizeof(snapshot_buf));

    FILE* f = fopen("snapshot_dump.bin", "wb");
    if (f) {
        fwrite(snapshot_buf, 1, snap_size, f);
        fclose(f);
        printf("Snapshot written: snapshot_dump.bin (%zu bytes)\n", snap_size);
        return 0;
    } else {
        perror("fopen");
        return 1;
    }
}
