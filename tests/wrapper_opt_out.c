#define MT_WRAP_DISABLE
#include "../include/mt_wrap.h"

#ifdef malloc
#error "malloc macro should be disabled"
#endif

int main(void)
{
    void* p = malloc(8);
    free(p);
    return p ? 0 : 1;
}
