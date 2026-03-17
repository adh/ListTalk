/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Condition__
#define H__ListTalk__Condition__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

#include <stdarg.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Condition);
LT_DECLARE_CLASS(LT_WarningCondition);
LT_DECLARE_CLASS(LT_ErrorCondition);

LT_Value LT_Condition_new(LT_Class* klass, const char* message, LT_Value args);
LT_Value LT_Condition_vnew(LT_Class* klass, const char* message, va_list args);
LT_Value LT_Condition_impl(const char* message, ...);
LT_Value LT_Warning_impl(const char* message, ...);
LT_Value LT_Error_impl(const char* message, ...);

#define LT_Condition(...) LT_Condition_impl(__VA_ARGS__, NULL)
#define LT_Warning(...) LT_Warning_impl(__VA_ARGS__, NULL)
#define LT_Error(...) LT_Error_impl(__VA_ARGS__, NULL)

LT__END_DECLS

#endif
