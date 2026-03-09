#ifndef MTV1_PLATFORM_H
#define MTV1_PLATFORM_H

/* Platform detection for conditional compilation */

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#  define MTV1_PLATFORM_WIN32 1
#  define MTV1_PLATFORM_POSIX 0
#else
#  define MTV1_PLATFORM_WIN32 0
#  define MTV1_PLATFORM_POSIX 1
#endif

#endif /* MTV1_PLATFORM_H */
