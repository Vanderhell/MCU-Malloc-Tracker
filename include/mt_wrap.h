#ifndef MT_WRAP_H
#define MT_WRAP_H

/*
 * mt_wrap.h
 *
 * Include this header last in a translation unit that should have malloc/free/
 * realloc wrapped. Do not include it in library implementation files or any TU
 * that must call the real allocator directly.
 *
 * Scope:
 * - The macros below affect only the current translation unit.
 * - Already-compiled third-party objects are not retroactively intercepted.
 * - Define MT_WRAP_DISABLE before including this header to suppress wrappers.
 */

#if defined(malloc) || defined(free) || defined(realloc)
#error "malloc/free/realloc must not be predefined before including mt_wrap.h."
#endif

#include <stdlib.h>
#include "mt_api.h"

#ifndef MT_WRAP_DISABLE
#define malloc(x)      mt_malloc((x), __FILE__, __LINE__)
#define free(p)        mt_free((p), __FILE__, __LINE__)
#define realloc(p, x)  mt_realloc((p), (x), __FILE__, __LINE__)
#endif

#endif /* MT_WRAP_H */
