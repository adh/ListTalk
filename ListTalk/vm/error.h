/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__error__
#define H__ListTalk__error__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>

#include <stdio.h>

LT__BEGIN_DECLS

/* This gets included pretty much everywhere, so it cannot use VM types in 
   declarations */

/** Report an error, trailing arguments are key-value pairs consisting of
 *  a const char* key and a LT_Value value. The list is terminated by NULL.
 */
void _Noreturn LT_error_impl(const char* message, ...);
#define LT_error(...) LT_error_impl(__VA_ARGS__, NULL)

void _Noreturn LT_system_error(const char* message, int errnum);
void _Noreturn LT_type_error(LT_Value value, LT_Class* expected_class);
void LT_print_backtrace(FILE* stream);

LT__END_DECLS

#endif
