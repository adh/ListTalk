/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__repl__repl__
#define H__ListTalk__repl__repl__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/vm/value.h>

LT__BEGIN_DECLS

typedef struct LT_REPL_State_s LT_REPL_State;

typedef void (*LT_REPL_Callback)(LT_Value object, void* context);

LT_REPL_State* LT_REPL_State_new(void);

/*
 * Set the primary prompt template.  "%p" expands to the name of the
 * current package and "%%" expands to a literal percent sign.
 */
void LT_REPL_State_set_prompt(LT_REPL_State* state, const char* prompt);

void LT_REPL_State_set_continuation_indent(
    LT_REPL_State* state,
    size_t indent
);

/* Return LT_INVALID at end of input. */
LT_Value LT_REPL_State_read(LT_REPL_State* state);

void LT_REPL_State_loop(
    LT_REPL_State* state,
    LT_REPL_Callback callback,
    void* context
);

LT__END_DECLS

#endif
