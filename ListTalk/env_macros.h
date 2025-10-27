#ifndef H__ListTalk__env_macros__
#define H__ListTalk__env_macros__

#if __STDC_VERSION__ < 201112L
#error "ListTalk requires a C11-compliant compiler"
#endif

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
#define LT__BEGIN_DECLS extern "C" {
#define LT__END_DECLS   }
#else
#define LT__BEGIN_DECLS
#define LT__END_DECLS
#endif

#endif