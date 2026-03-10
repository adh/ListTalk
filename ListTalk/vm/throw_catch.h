/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__throw_cacth__
#define H__ListTalk__throw_cacth__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>

#include <setjmp.h>

LT__BEGIN_DECLS

typedef struct LT_ThrowCatchFrame_s {
    jmp_buf jump_buffer;
    LT_Value tag;
    LT_Value thrown_tag;
    LT_Value thrown_value;
    int catches_all;
    struct LT_ThrowCatchFrame_s* previous;
} LT_ThrowCatchFrame;

extern _Thread_local LT_ThrowCatchFrame* LT__throw_catch_stack;

_Noreturn void LT_throw(LT_Value tag, LT_Value value);

#define LT_CATCH(TAG_EXPR, RESULT_LVALUE, BODY) \
    do { \
        LT_ThrowCatchFrame LT__throw_catch_frame; \
        int LT__throw_catch_jump_value; \
        LT__throw_catch_frame.tag = (TAG_EXPR); \
        LT__throw_catch_frame.thrown_tag = LT_NIL; \
        LT__throw_catch_frame.thrown_value = LT_NIL; \
        LT__throw_catch_frame.catches_all = 0; \
        LT__throw_catch_frame.previous = LT__throw_catch_stack; \
        LT__throw_catch_stack = &LT__throw_catch_frame; \
        LT__throw_catch_jump_value = setjmp(LT__throw_catch_frame.jump_buffer); \
        if (LT__throw_catch_jump_value == 0) { \
            BODY \
        } else { \
            (RESULT_LVALUE) = LT__throw_catch_frame.thrown_value; \
        } \
        LT__throw_catch_stack = LT__throw_catch_frame.previous; \
    } while (0)

#define LT_UNWIND_PROTECT(PROTECTED, CLEANUP) \
    do { \
        LT_ThrowCatchFrame LT__throw_catch_frame; \
        int LT__throw_catch_jump_value; \
        int LT__throw_catch_rethrow = 0; \
        LT_Value LT__throw_catch_rethrow_tag = LT_NIL; \
        LT_Value LT__throw_catch_rethrow_value = LT_NIL; \
        LT__throw_catch_frame.tag = LT_NIL; \
        LT__throw_catch_frame.thrown_tag = LT_NIL; \
        LT__throw_catch_frame.thrown_value = LT_NIL; \
        LT__throw_catch_frame.catches_all = 1; \
        LT__throw_catch_frame.previous = LT__throw_catch_stack; \
        LT__throw_catch_stack = &LT__throw_catch_frame; \
        LT__throw_catch_jump_value = setjmp(LT__throw_catch_frame.jump_buffer); \
        if (LT__throw_catch_jump_value == 0) { \
            PROTECTED \
        } else { \
            LT__throw_catch_rethrow = 1; \
            LT__throw_catch_rethrow_tag = LT__throw_catch_frame.thrown_tag; \
            LT__throw_catch_rethrow_value = LT__throw_catch_frame.thrown_value; \
        } \
        LT__throw_catch_stack = LT__throw_catch_frame.previous; \
        CLEANUP \
        if (LT__throw_catch_rethrow) { \
            LT_throw(LT__throw_catch_rethrow_tag, LT__throw_catch_rethrow_value); \
        } \
    } while (0)

LT__END_DECLS

#endif
