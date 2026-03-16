/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__stack_trace__
#define H__ListTalk__stack_trace__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/vm/Environment.h>
#include <ListTalk/classes/Pair.h>

#include <stdio.h>

LT__BEGIN_DECLS

#define LT_STACK_FRAME_TYPE_EVAL 1
#define LT_STACK_FRAME_TYPE_APPLY 2

typedef union LT_StackFrameArguments_s {
    struct {
        LT_Value expression;
        LT_Environment* environment;
    } eval;
    struct {
        LT_Value callable;
        LT_Value arguments;
    } apply;
} LT_StackFrameArguments;

typedef struct LT_StackFrame_s {
    int type;
    LT_StackFrameArguments arguments;
    struct LT_StackFrame_s* previous;
} LT_StackFrame;

extern _Thread_local LT_StackFrame* LT__stack_trace_stack;

static inline void LT_stack_trace_push(LT_StackFrame* frame){
    frame->previous = LT__stack_trace_stack;
    LT__stack_trace_stack = frame;
}

static inline void LT_stack_trace_pop(LT_StackFrame* frame){
    if (LT__stack_trace_stack == frame){
        LT__stack_trace_stack = frame->previous;
    }
}

static inline LT_StackFrame* LT_stack_trace_top(void){
    return LT__stack_trace_stack;
}

static inline void LT_stack_trace_restore(LT_StackFrame* frame){
    LT__stack_trace_stack = frame;
}

unsigned int LT_stack_trace_depth(void);
LT_Value LT_stack_trace_capture(void);
void LT_stack_trace_print(FILE* stream);

LT__END_DECLS

#endif
