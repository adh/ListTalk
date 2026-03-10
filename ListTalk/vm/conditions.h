/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__conditions__
#define H__ListTalk__conditions__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/vm/value.h>
#include <ListTalk/vm/throw_catch.h>

LT__BEGIN_DECLS

typedef struct LT_ConditionHandlerFrame_s {
    LT_Value handler;
    struct LT_ConditionHandlerFrame_s* previous;
} LT_ConditionHandlerFrame;

extern _Thread_local LT_ConditionHandlerFrame* LT__condition_handler_stack;

void LT_signal(LT_Value condition);

#define LT_HANDLER_BIND(HANDLER_EXPR, BODY) \
    do { \
        LT_ConditionHandlerFrame LT__condition_handler_frame; \
        LT__condition_handler_frame.handler = (HANDLER_EXPR); \
        LT__condition_handler_frame.previous = LT__condition_handler_stack; \
        LT__condition_handler_stack = &LT__condition_handler_frame; \
        LT_UNWIND_PROTECT( \
        { \
            BODY \
        }, \
        { \
            LT__condition_handler_stack = LT__condition_handler_frame.previous; \
        }); \
    } while (0)

LT__END_DECLS

#endif
