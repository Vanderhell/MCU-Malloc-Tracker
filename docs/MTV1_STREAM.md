# MTV1 Streamer — MCU Malloc Tracker

## Overview

MTV1 streamer provides a callback-based API for sending heap snapshots, telemetry, and marks over MTV1 protocol. Designed for embedded systems with minimal overhead (no malloc, deterministic).

## MTV1 Frame Format

Each frame is 20-byte header + variable payload (0–512 bytes).

```
[0-3]   magic           "MTV1" (ASCII)
[4]     version         1 (uint8)
[5]     type            1=SNAPSHOT 2=TELEMETRY 3=MARK 4=END (uint8)
[6-7]   flags           reserved (uint16_le)
[8-11]  seq             monotonic sequence (uint32_le)
[12-15] payload_len     bytes in payload (uint32_le)
[16-19] crc32           IEEE CRC32 (uint32_le)
[20+]   payload         0–512 bytes
```

### Frame Types

| Type | Name | Payload | Purpose |
|------|------|---------|---------|
| 1 | SNAPSHOT_MTS1 | MTS1 binary (≥40 bytes) | Heap snapshot |
| 2 | TELEMETRY_TEXT | UTF-8 text | Debug message |
| 3 | MARK_TEXT | UTF-8 text | Event marker (e.g., "bootup", "crash") |
| 4 | END | (empty) | Stream termination |

## CRC32/IEEE Contract

- **Polynomial**: 0xEDB88320 (reflected, IEEE standard)
- **Init**: 0xFFFFFFFF
- **Process**: header (20 bytes with crc32 field zeroed) + payload
- **Final XOR**: 0xFFFFFFFF applied once after all data

Example (C):
```c
uint32_t crc = 0xFFFFFFFFu;
crc = mt_crc32_ieee_update(header_buf, 20, crc);  // header with crc=0
crc = mt_crc32_ieee_update(payload, payload_len, crc);
crc ^= 0xFFFFFFFFu;
```

## API

### Initialize Streamer

```c
mtv1_stream_t* stream = mtv1_stream_init(mtv1_tx_fn tx_fn, void* tx_ctx);
```

- **tx_fn**: Callback that transmits bytes. Returns bytes written (≥0) or -1 on error.
- **tx_ctx**: User data passed to callback.
- **Returns**: Opaque handle, or NULL if tx_fn is NULL.

Sequence counter starts at 1. Each frame automatically increments seq.

### Send Snapshot

```c
int mtv1_send_snapshot(mtv1_stream_t* stream, const uint8_t* snapshot_buf, uint32_t snapshot_len);
```

Sends MTS1 binary snapshot (≥40 bytes). Returns 0 on success, -1 on error (too large, null pointer, etc.).

### Send Telemetry

```c
int mtv1_send_telemetry_line(mtv1_stream_t* stream, const char* text);
```

Sends text telemetry (max 512 bytes). Returns 0 on success, -1 on error.

### Send Mark

```c
int mtv1_send_mark(mtv1_stream_t* stream, const char* label);
```

Sends event mark (e.g., "bootup", "checkpoint", "crash"). Max 512 bytes. Returns 0 on success, -1 on error.

### Send End Frame

```c
int mtv1_send_end(mtv1_stream_t* stream);
```

Sends END frame to signal stream termination. Returns 0 on success, -1 on error.

### Get Next Sequence

```c
uint32_t mtv1_stream_next_seq(mtv1_stream_t* stream);
```

Returns current seq and increments counter. Useful for debugging or tracing frame order. Normally called automatically by send functions.

### Clean Up

```c
void mtv1_stream_free(mtv1_stream_t* stream);
```

No-op (no malloc), but provided for API consistency.

## Integration Example

```c
#include "mtv1_config.h"
#include "mtv1_stream.h"
#include "mt_api.h"

/* Serial TX callback */
int uart_tx(const uint8_t* data, size_t len, void* ctx)
{
    /* Send data over UART */
    for (size_t i = 0; i < len; i++) {
        uart_putc(data[i]);
    }
    return (int)len;
}

void main(void)
{
    /* Initialize MCU Malloc Tracker */
    mt_init();

    /* Initialize MTV1 streamer */
    mtv1_stream_t* stream = mtv1_stream_init(uart_tx, NULL);

    /* Allocate and track */
    void* p = mt_malloc(128);

    /* Send snapshot */
    uint8_t snap_buf[512];
    size_t snap_len = mt_snapshot_write(snap_buf, sizeof(snap_buf));
    mtv1_send_snapshot(stream, snap_buf, (uint32_t)snap_len);

    /* Send telemetry */
    mtv1_send_telemetry_line(stream, "allocation successful");

    /* Send mark */
    mtv1_send_mark(stream, "checkpoint1");

    /* Cleanup */
    mt_free(p);
    mtv1_stream_free(stream);
}
```

## Configuration

In `include/mtv1_config.h`:

```c
#define MTV1_ENABLE 1                  /* Enable streamer */
#define MTV1_TX_MAX_PAYLOAD 512         /* Max bytes per frame payload */
```

## Determinism

- Sequence numbers: monotonic 1..UINT32_MAX (no timestamps)
- CRC32: bit-by-bit calculation (no lookup table, deterministic)
- No malloc, no floating-point, no OS calls
- Stateless transmit callback (no hidden buffering)

## Notes

- MTV1 streamer requires `MT_ENABLE_SNAPSHOT` to be enabled in mt_config.h
- Frames are self-contained; no state needed on receiver side for CRC verification
- For serial transport, consider flow control and error recovery at higher layer
- Payload must fit in 512 bytes (MTV1_TX_MAX_PAYLOAD)
