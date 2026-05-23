/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__REPL__
#define H__ListTalk__REPL__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/Environment.h>
#include <ListTalk/vm/tail_call.h>
#include <ListTalk/vm/value.h>

LT__BEGIN_DECLS

typedef int (*LTREPL_EvaluateFunction)(
    const char* source,
    void* baton,
    LT_Value* caught_condition
);

typedef struct LTREPL_Interaction_s {
    const char* primary_prompt;
    const char* continuation_prompt;
    LTREPL_EvaluateFunction evaluate;
    LTREPL_EvaluateFunction evaluate_at_eof;
    void* baton;
} LTREPL_Interaction;

void LTREPL_init(void);
LT_Value LTREPL_errorTag(void);
void LTREPL_printCondition(LT_Value condition);
LT_Value LTREPL_replErrorHandler(
    LT_Value arguments,
    LT_Value invocation_context_kind,
    LT_Value invocation_context_data,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
);
LT_Value LTREPL_scriptErrorHandler(
    LT_Value arguments,
    LT_Value invocation_context_kind,
    LT_Value invocation_context_data,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
);
int LTREPL_evalSourceString(
    const char* source,
    LT_Value error_handler,
    LT_Environment* environment,
    int print_result,
    LT_Value* caught_condition
);
int LTREPL_interact(LTREPL_Interaction* interaction);
int LTREPL_eval(
    LT_Value error_handler,
    LT_Value eof_error_handler,
    LT_Environment* environment
);

LT__END_DECLS

#endif
