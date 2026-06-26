#ifndef WRAPPER_MULTITU_H
#define WRAPPER_MULTITU_H

#include <stddef.h>

void* mt_test_wrapped_alloc(size_t size);
void mt_test_wrapped_free(void* ptr);
void* mt_test_plain_alloc(size_t size);
void mt_test_plain_free(void* ptr);

#endif /* WRAPPER_MULTITU_H */
