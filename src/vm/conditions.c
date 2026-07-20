/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/conditions.h>
#include <ListTalk/ListTalk.h>
#include <ListTalk/utils.h>

void LT_signal(LT_Value condition){
    LT_ConditionHandlerFrame* frame = LT__condition_handler_stack;

    while (frame != NULL){
        (void)LT_apply(frame->handler, LT_cons(condition, LT_NIL), LT_NIL, LT_NIL, NULL);
        frame = frame->previous;
    }
}

LT_Value LT_current_restarts(void){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    LT_RestartFrame* frame = LT__restart_stack;

    while (frame != NULL){
        LT_ListBuilder_append(builder, frame->restart);
        frame = frame->previous;
    }
    return LT_ListBuilder_value(builder);
}

LT_Value LT_find_restart(LT_Value name){
    LT_RestartFrame* frame = LT__restart_stack;

    while (frame != NULL){
        LT_Restart* restart = LT_Restart_from_value(frame->restart);
        if (LT_Restart_name(restart) == name){
            return frame->restart;
        }
        frame = frame->previous;
    }
    return LT_NIL;
}

LT_Value LT_invoke_restart(LT_Value name, LT_Value arguments){
    LT_Value restart_value;
    LT_Restart* restart;

    if (!LT_List_proper_p(arguments)){
        LT_error("LT_invoke_restart expects proper argument list");
    }

    restart_value = LT_find_restart(name);
    if (restart_value == LT_NIL){
        LT_error("No active restart with requested name");
    }

    restart = LT_Restart_from_value(restart_value);
    return LT_apply(
        LT_Restart_callable(restart),
        arguments,
        LT_NIL,
        LT_NIL,
        NULL
    );
}
