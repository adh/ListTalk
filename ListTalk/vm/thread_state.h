/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__thread_state__
#define H__ListTalk__thread_state__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/vm/value.h>

LT__BEGIN_DECLS

typedef struct LT_ConditionHandlerFrame_s LT_ConditionHandlerFrame;
typedef struct LT_RestartFrame_s LT_RestartFrame;
typedef struct LT_StackFrame_s LT_StackFrame;
typedef struct LT_ThrowCatchFrame_s LT_ThrowCatchFrame;
typedef struct LT_Package_s LT_Package;
typedef struct LT_Thread_s LT_Thread;
typedef struct LT_WeakKeyIdentityDictionary_s LT_WeakKeyIdentityDictionary;
typedef struct LT_ThreadState_s LT_ThreadState;

struct LT_ThreadState_s {
    LT_ConditionHandlerFrame* condition_handler_stack;
    LT_RestartFrame* restart_stack;
    LT_StackFrame* stack_trace_stack;
    LT_ThrowCatchFrame* throw_catch_stack;
    LT_WeakKeyIdentityDictionary* dynamic_values;
    LT_Package* current_package;
    LT_Thread* current_thread;
    int current_package_is_set;
};

extern _Thread_local LT_ThreadState* LT__thread_state;

LT_ThreadState* LT_thread_state_slow(void);

static inline LT_ThreadState* LT_thread_state(void)
{
    if (LT__thread_state != NULL){
        return LT__thread_state;
    }
    return LT_thread_state_slow();
}

LT__END_DECLS

#endif
