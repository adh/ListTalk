/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Symbol__
#define H__ListTalk__Symbol__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/classes/Package.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Symbol);

extern LT_Value LT_Symbol_new(char* name);
extern LT_Value LT_Symbol_new_in(LT_Package* package, char* name);
extern LT_Value LT_Symbol_parse_token(char* token);
extern char* LT_Symbol_name(LT_Symbol* symbol);
extern LT_Package* LT_Symbol_package(LT_Symbol* symbol);

static inline int LT_Value_is_symbol(LT_Value value){
    return LT_VALUE_IS_POINTER(value)
        && LT_VALUE_POINTER_TAG(value) == LT_VALUE_POINTER_TAG_SYMBOL;
}

LT__END_DECLS

#endif
