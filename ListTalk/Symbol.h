#ifndef H__ListTalk__Symbol__
#define H__ListTalk__Symbol__

#include <ListTalk/env_macros.h>

#include <ListTalk/OOP.h>

LT__CXX_GUARD_BEGIN

extern LT_Class LT_Symbol_class;

typedef struct LT_Symbol LK_Symbol;

extern LT_Symbol* LT_Symbol_new(char* name);
extern char* LT_Symbol_name(LT_Symbol* symbol);

LT__CXX_GUARD_END

#endif