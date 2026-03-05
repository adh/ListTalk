#ifndef H__ListTalk__Symbol__
#define H__ListTalk__Symbol__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Symbol);

extern LT_Value LT_Symbol_new(char* name);
extern char* LT_Symbol_name(LT_Symbol* symbol);

static inline int LT_Value_is_symbol(LT_Value value){
    return LT_VALUE_IS_POINTER(value)
        && LT_VALUE_POINTER_TAG(value) == LT_VALUE_POINTER_TAG_SYMBOL;
}

LT__END_DECLS

#endif
