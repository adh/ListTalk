#ifndef H__ListTalk__error__
#define H__ListTalk__error__

#include <ListTalk/env_macros.h>

#include <ListTalk/value.h>

LT_BEGIN_DECLS

/* This gets included pretty much everywhere, so it cannot use VM types in 
   declarations */

/** Report an error, trailing arguments are key-value pairs consisting of
 *  a const char* key and a LT_Object value. The list is terminated by NULL.
 */
void LT_error(const char* message, ...);

void LT_type_error(LT_Value value, LT_Class* expected_class);

LT_END_DECLS

#endif