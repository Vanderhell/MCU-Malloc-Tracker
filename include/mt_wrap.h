/**
 * @file mt_wrap.h
 * @brief One-file integration wrapper — drop-in malloc/free/realloc replacement
 *
 * Usage: Include this instead of manually wrapping malloc/free/realloc
 *
 * #include "mt_wrap.h"
 *
 * All malloc/free/realloc calls will be automatically tracked with file/line info.
 */

#pragma once

#include "mt_api.h"

/* Drop-in replacements with automatic __FILE__/__LINE__ tracking */
#define malloc(x)       mt_malloc((x), __FILE__, __LINE__)
#define free(p)         mt_free((p), __FILE__, __LINE__)
#define realloc(p, x)   mt_realloc((p), (x), __FILE__, __LINE__)

#endif /* MT_WRAP_H */
