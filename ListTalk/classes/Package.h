/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Package__
#define H__ListTalk__Package__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Package);

LT_Package* LT_Package_new(char* name);
char* LT_Package_name(LT_Package* package);
LT_Value LT_Package_intern_symbol(LT_Package* package, char* name);

extern LT_Package LT_Package_LISTTALK;
extern LT_Package LT_Package_KEYWORD;

#define LT_PACKAGE_LISTTALK (&LT_Package_LISTTALK)
#define LT_PACKAGE_KEYWORD (&LT_Package_KEYWORD)

LT__END_DECLS

#endif
