#ifndef H__ListTalk__env_macros__
#define H__ListTalk__env_macros__

#ifndef _REENTRANT
#  define _REENTRANT
#endif
#ifndef GC_THREADS
#  define GC_THREADS
#endif

#include <stdint.h>
#include <pthread.h>
#include <gc/gc.h>

#ifdef __cplusplus
#define LT__CXX_GUARD_BEGIN extern "C" {
#define LT__CXX_GUARD_END   }
#else
#define LT__CXX_GUARD_BEGIN
#define LT__CXX_GUARD_END
#endif

#endif