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

#define LT_PACKAGE_LISTTALK (LT_Package_new("ListTalk"))
#define LT_PACKAGE_KEYWORD (LT_Package_new("keyword"))

LT__END_DECLS

#endif
