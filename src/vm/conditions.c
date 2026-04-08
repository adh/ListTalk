/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/conditions.h>
#include <ListTalk/ListTalk.h>

_Thread_local LT_ConditionHandlerFrame* LT__condition_handler_stack = NULL;

void LT_signal(LT_Value condition){
    LT_ConditionHandlerFrame* frame = LT__condition_handler_stack;

    while (frame != NULL){
        (void)LT_apply(frame->handler, LT_cons(condition, LT_NIL), LT_NIL, LT_NIL, NULL);
        frame = frame->previous;
    }
}
