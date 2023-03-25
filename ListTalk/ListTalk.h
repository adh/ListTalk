#ifndef H__ListTalk__ListTalk__
#define H__ListTalk__ListTalk__

#ifndef _REENTRANT
#  define _REENTRANT
#endif
#ifndef GC_THREADS
#  define GC_THREADS
#endif

#include <stdint.h>
#include <pthread.h>
#include <gc/gc.h>

#include <ListTalk/OOP.h>

#ifdef __cplusplus
extern "C" {
#endif

extern LT_Object* LT_eval(LT_Object* expression);

#ifdef __cplusplus
}
#endif


#endif