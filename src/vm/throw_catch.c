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

    while (frame != NULL){
        if (frame->catches_all || frame->tag == tag){
            frame->thrown_tag = tag;
            frame->thrown_value = value;
            longjmp(frame->jump_buffer, 1);
        }
        frame = frame->previous;
    }

    LT_error("Uncaught throw");
    abort();
}
