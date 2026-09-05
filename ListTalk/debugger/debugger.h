/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__debugger__debugger__
#define H__ListTalk__debugger__debugger__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/classes/Environment.h>
#include <ListTalk/vm/value.h>

LT__BEGIN_DECLS

/*
 * Enter a debugger REPL for CONDITION in a fresh lexical environment.
 * DEBUGGER_HOOK is dynamically reinstalled while evaluating REPL forms.
 */
void LT_Debugger_break(LT_Value condition, LT_Value debugger_hook);

/* Return the two-argument primitive suitable for use as debugger_hook. */
LT_Value LT_Debugger_get_hook(void);

/* Install LT_Debugger_get_hook() as the current thread's debugger hook. */
void LT_Debugger_enable(void);

/* Bind the interactive inspector primitive as ListTalk:inspect. */
void LT_Debugger_define_inspect(LT_Environment* environment);

/* Interactively display OBJECT and descend through its named slots. */
void LT_Debugger_inspect(LT_Value object);

LT__END_DECLS

#endif
