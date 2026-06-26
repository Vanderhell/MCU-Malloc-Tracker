#define MT_WRAP_DISABLE
#include "../include/mt_wrap.h"
#include "wrapper_multitu.h"

void* mt_test_plain_alloc(size_t size)
{
    return malloc(size);
}

void mt_test_plain_free(void* ptr)
{
    free(ptr);
}
