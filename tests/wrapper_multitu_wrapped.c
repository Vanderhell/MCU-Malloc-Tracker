#include "../include/mt_wrap.h"
#include "wrapper_multitu.h"

void* mt_test_wrapped_alloc(size_t size)
{
    return malloc(size);
}

void mt_test_wrapped_free(void* ptr)
{
    free(ptr);
}
