/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */
#ifndef H__ListTalk__Pathname__
#define H__ListTalk__Pathname__

#include <ListTalk/classes/String.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

typedef struct LT_Pathname_s LT_Pathname;
typedef struct LT_RelativePathname_s LT_RelativePathname;
typedef struct LT_AbsolutePathname_s LT_AbsolutePathname;

extern LT_Class LT_Pathname_class;
extern LT_Class LT_Pathname_class_class;
LT_DECLARE_CLASS(LT_RelativePathname);
LT_DECLARE_CLASS(LT_AbsolutePathname);

static inline int LT_Pathname_p(LT_Value value){
    return LT_Value_is_instance_of(value, LT_STATIC_CLASS(LT_Pathname));
}

static inline LT_Pathname* LT_Pathname_from_value(LT_Value value){
    if (!LT_Pathname_p(value)){
        LT_type_error(value, &LT_Pathname_class);
    }
    return (LT_Pathname*)LT_VALUE_POINTER_VALUE(value);
}

LT_Pathname* LT_Pathname_new(char* pathname);
LT_Pathname* LT_Pathname_from_string(LT_String* string);
LT_RelativePathname* LT_RelativePathname_new(char* pathname);
LT_RelativePathname* LT_RelativePathname_from_string(LT_String* string);
LT_AbsolutePathname* LT_AbsolutePathname_new(char* pathname);
LT_AbsolutePathname* LT_AbsolutePathname_from_string(LT_String* string);
LT_Pathname* LT_Pathname_append(LT_Pathname* left, LT_Pathname* right);
LT_String* LT_Pathname_as_string(LT_Pathname* pathname);
char* LT_Pathname_value_cstr(LT_Pathname* pathname);
int LT_Pathname_absolute_p(LT_Pathname* pathname);
int LT_Pathname_relative_p(LT_Pathname* pathname);
char* LT_Pathname_like_value_cstr(LT_Value value);
LT_String* LT_Pathname_like_as_string(LT_Value value);

LT__END_DECLS
#endif
