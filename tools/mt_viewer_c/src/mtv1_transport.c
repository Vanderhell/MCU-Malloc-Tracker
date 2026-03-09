#include "mtv1_transport.h"
#include <stdlib.h>

void mtv1_transport_close(mtv1_transport_t* t) {
    if (!t) return;
    if (t->close)
        t->close(t);
}
