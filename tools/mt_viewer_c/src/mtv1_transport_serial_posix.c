#include "mtv1_transport.h"
#include "mtv1_platform.h"

#if MTV1_PLATFORM_POSIX

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int fd;
} posix_ctx_t;

/* Convert baud rate to termios constant */
static speed_t baud_to_speed(uint32_t baud) {
    switch (baud) {
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        case 921600: return B921600;
        default:     return B9600;
    }
}

static int posix_read(mtv1_transport_t* t, uint8_t* buf, size_t len,
                      int timeout_ms) {
    if (!t || !t->ctx) return -1;

    posix_ctx_t* ctx = (posix_ctx_t*)t->ctx;

    /* Set up select timeout */
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(ctx->fd, &readfds);

    int sel = select(ctx->fd + 1, &readfds, NULL, NULL, &tv);
    if (sel <= 0)
        return (sel < 0) ? -1 : 0;  /* -1=error, 0=timeout */

    ssize_t n = read(ctx->fd, buf, len);
    return (n < 0) ? -1 : (int)n;
}

static void posix_close(mtv1_transport_t* t) {
    if (!t || !t->ctx) return;

    posix_ctx_t* ctx = (posix_ctx_t*)t->ctx;
    if (ctx->fd >= 0)
        close(ctx->fd);

    free(ctx);
    free(t);
}

mtv1_transport_t* mtv1_transport_serial_open_posix(
    const char* device,
    uint32_t    baud) {

    if (!device) return NULL;

    int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0)
        return NULL;

    /* Configure terminal */
    struct termios tios, orig_tios;
    tcgetattr(fd, &orig_tios);
    tios = orig_tios;

    /* Raw mode */
    tios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tios.c_oflag &= ~OPOST;
    tios.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tios.c_cflag &= ~(CSIZE | PARENB);
    tios.c_cflag |= CS8;

    /* Baud rate */
    speed_t speed = baud_to_speed(baud);
    cfsetispeed(&tios, speed);
    cfsetospeed(&tios, speed);

    /* Blocking reads */
    tios.c_cc[VMIN] = 0;
    tios.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tios) < 0) {
        close(fd);
        return NULL;
    }

    /* Allocate transport structure */
    mtv1_transport_t* t = (mtv1_transport_t*)malloc(sizeof(*t));
    posix_ctx_t* ctx = (posix_ctx_t*)malloc(sizeof(*ctx));

    t->read = posix_read;
    t->close = posix_close;
    t->ctx = ctx;
    ctx->fd = fd;

    return t;
}

#else
/* Stub for non-POSIX platforms */
mtv1_transport_t* mtv1_transport_serial_open_posix(
    const char* device,
    uint32_t    baud) {
    (void)device;
    (void)baud;
    return NULL;
}
#endif
