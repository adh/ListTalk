#ifndef H__ListTalk__error__
#define H__ListTalk__error__

#include <ListTalk/env_macros.h>

LT_BEGIN_DECLS

/* This gets included pretty much everywhere, so it cannot use VM types in 
   declarations */

/** Report an error, trailing arguments are key-value pairs consisting of
 *  a const char* key and a LT_Object value. The list is terminated by NULL.
 */
void LT_error(const char* message, ...);


LT_END_DECLS

#endif