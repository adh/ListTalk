/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/throw_catch.h>
#include <ListTalk/vm/error.h>

#include <setjmp.h>
#include <stdlib.h>

_Thread_local LT_ThrowCatchFrame* LT__throw_catch_stack = NULL;

_Noreturn void LT_throw(LT_Value tag, LT_Value value){
    LT_ThrowCatchFrame* frame = LT__throw_catch_stack;
    int has_matching_tag = 0;

    while (frame != NULL){
        if (!frame->is_unwind_protect && frame->tag == tag){
            has_matching_tag = 1;
            break;
        }
        frame = frame->previous;
    }

    if (!has_matching_tag){
        LT_error("Uncaught throw");
    }

    frame = LT__throw_catch_stack;

    while (frame != NULL){
        if (frame->is_unwind_protect || frame->tag == tag){
            frame->thrown_tag = tag;
            frame->thrown_value = value;
            longjmp(frame->jump_buffer, 1);
        }
        frame = frame->previous;
    }

    LT_error("Uncaught throw");
}
