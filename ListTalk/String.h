#ifndef H__ListTalk__String__
#define H__ListTalk__String__

#include <ListTalk/OOP.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_String);

LT_String* LT_String_new(char* buf, size_t len);
LT_String* LT_String_new_cstr(char* str);
char* LT_String_value_cstr(LT_String* string);

LT__END_DECLS

#endif