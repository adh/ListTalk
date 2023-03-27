#ifndef H__ListTalk__printer__
#define H__ListTalk__printer__

#include <ListTalk/env_macros.h>
#include <ListTalk/decl_macros.h>
#include <ListTalk/OOP.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Printer);

extern void LT_printer_print_object(LT_Object* object);

LT__END_DECLS

#endif