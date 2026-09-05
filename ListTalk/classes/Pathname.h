/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */
#ifndef H__ListTalk__Pathname__
#define H__ListTalk__Pathname__

#include <ListTalk/classes/String.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Pathname);

LT_Pathname* LT_Pathname_new(char* pathname);
LT_Pathname* LT_Pathname_from_string(LT_String* string);
LT_String* LT_Pathname_as_string(LT_Pathname* pathname);
char* LT_Pathname_value_cstr(LT_Pathname* pathname);
char* LT_Pathname_like_value_cstr(LT_Value value);
LT_String* LT_Pathname_like_as_string(LT_Value value);

LT__END_DECLS
#endif
