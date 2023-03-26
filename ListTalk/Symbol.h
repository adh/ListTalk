#ifndef H__ListTalk__Symbol__
#define H__ListTalk__Symbol__

#include <ListTalk/env_macros.h>

#include <ListTalk/OOP.h>
#include <ListTalk/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Symbol);

extern LT_Symbol* LT_Symbol_new(char* name);
extern char* LT_Symbol_name(LT_Symbol* symbol);

LT__END_DECLS

#endif