#include "mtv1_transport.h"
#include "mtv1_platform.h"

#if MTV1_PLATFORM_WIN32

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    HANDLE hPort;
} win32_ctx_t;

static int win32_read(mtv1_transport_t* t, uint8_t* buf, size_t len,
                      int timeout_ms) {
    if (!t || !t->ctx) return -1;

    win32_ctx_t* ctx = (win32_ctx_t*)t->ctx;
    DWORD bytes_read = 0;

    /* Set timeout */
    COMMTIMEOUTS timeouts;
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = timeout_ms;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    SetCommTimeouts(ctx->hPort, &timeouts);

    if (!ReadFile(ctx->hPort, buf, (DWORD)len, &bytes_read, NULL))
        return -1;

    return (int)bytes_read;
}

static void win32_close(mtv1_transport_t* t) {
    if (!t || !t->ctx) return;

    win32_ctx_t* ctx = (win32_ctx_t*)t->ctx;
    if (ctx->hPort != INVALID_HANDLE_VALUE)
        CloseHandle(ctx->hPort);

    free(ctx);
    free(t);
}

mtv1_transport_t* mtv1_transport_serial_open_win32(
    const char* port,
    uint32_t    baud) {

    if (!port) return NULL;

    /* Open COM port */
    HANDLE hPort = CreateFileA(
        port,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hPort == INVALID_HANDLE_VALUE)
        return NULL;

    /* Configure COM port */
    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);

    if (!GetCommState(hPort, &dcb)) {
        CloseHandle(hPort);
        return NULL;
    }

    dcb.BaudRate = baud;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;

    if (!SetCommState(hPort, &dcb)) {
        CloseHandle(hPort);
        return NULL;
    }

    /* Allocate transport structure */
    mtv1_transport_t* t = (mtv1_transport_t*)malloc(sizeof(*t));
    win32_ctx_t* ctx = (win32_ctx_t*)malloc(sizeof(*ctx));

    t->read = win32_read;
    t->close = win32_close;
    t->ctx = ctx;
    ctx->hPort = hPort;

    return t;
}

#else
/* Stub for non-Windows platforms */
mtv1_transport_t* mtv1_transport_serial_open_win32(
    const char* port,
    uint32_t    baud) {
    (void)port;
    (void)baud;
    return NULL;
}
#endif
