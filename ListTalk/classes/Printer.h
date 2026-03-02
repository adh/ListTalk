#ifndef H__ListTalk__printer__
#define H__ListTalk__printer__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/value.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Printer);

extern void LT_printer_print_object(LT_Value object);

LT__END_DECLS

#endif