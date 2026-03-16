/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__StackFrame__
#define H__ListTalk__StackFrame__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/vm/Environment.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_StackFrameObject);
LT_DECLARE_CLASS(LT_EvalStackFrame);
LT_DECLARE_CLASS(LT_ApplyStackFrame);

LT_EvalStackFrame* LT_EvalStackFrame_new(
    LT_Value expression,
    LT_Environment* environment
);
LT_Value LT_EvalStackFrame_expression(LT_EvalStackFrame* frame);
LT_Environment* LT_EvalStackFrame_environment(LT_EvalStackFrame* frame);

LT_ApplyStackFrame* LT_ApplyStackFrame_new(LT_Value callable, LT_Value arguments);
LT_Value LT_ApplyStackFrame_callable(LT_ApplyStackFrame* frame);
LT_Value LT_ApplyStackFrame_arguments(LT_ApplyStackFrame* frame);

LT__END_DECLS

#endif
