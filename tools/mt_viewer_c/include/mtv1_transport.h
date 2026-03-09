#ifndef MTV1_TRANSPORT_H
#define MTV1_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>

typedef struct mtv1_transport mtv1_transport_t;

/* Abstract read interface — returns bytes read, 0=timeout, -1=error */
typedef int (*mtv1_transport_read_fn)(mtv1_transport_t* t, uint8_t* buf,
                                      size_t len, int timeout_ms);
typedef void (*mtv1_transport_close_fn)(mtv1_transport_t* t);

struct mtv1_transport {
    mtv1_transport_read_fn  read;
    mtv1_transport_close_fn close;
    void*                   ctx;     /* platform-specific handle */
};

/* Platform-specific openers */
mtv1_transport_t* mtv1_transport_serial_open_win32(
    const char* port,   /* e.g. "COM5" */
    uint32_t    baud
);

mtv1_transport_t* mtv1_transport_serial_open_posix(
    const char* device, /* e.g. "/dev/ttyUSB0" */
    uint32_t    baud
);

/* Common close (through vtable) */
void mtv1_transport_close(mtv1_transport_t* t);

#endif /* MTV1_TRANSPORT_H */
