/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__debugger__debugger__
#define H__ListTalk__debugger__debugger__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/vm/value.h>

LT__BEGIN_DECLS

/* Enter a debugger REPL for CONDITION in a fresh lexical environment. */
void LT_Debugger_break(LT_Value condition);

/* Interactively display OBJECT and descend through its named slots. */
void LT_Debugger_inspect(LT_Value object);

LT__END_DECLS

#endif
