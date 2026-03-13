/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Package__
#define H__ListTalk__Package__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/vm/throw_catch.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Package);

LT_Package* LT_Package_new(char* name);
LT_Package* LT_Package_find(char* name);
char* LT_Package_name(LT_Package* package);
LT_Value LT_Package_used_packages(LT_Package* package);
LT_Value LT_Package_intern_symbol(LT_Package* package, char* name);
LT_Value LT_Package_intern_local_symbol(LT_Package* package, char* name);
LT_Value LT_Package_lookup_local_symbol(LT_Package* package, char* name);
int LT_Package_uses_package(LT_Package* package, LT_Package* used_package);
void LT_Package_use_package(LT_Package* package,
                            LT_Package* used_package,
                            char* nickname);
LT_Package* LT_Package_resolve_used_package(LT_Package* package, char* name);

LT_Package* LT_get_current_package(void);
void LT_set_current_package(LT_Package* package);

#define LT_WITH_PACKAGE(PACKAGE_EXPR, BODY) \
    do { \
        LT_Package* LT__with_package_previous = LT_get_current_package(); \
        LT_set_current_package((PACKAGE_EXPR)); \
        LT_UNWIND_PROTECT( \
        { \
            BODY \
        }, \
        { \
            LT_set_current_package(LT__with_package_previous); \
        }); \
    } while (0)

extern LT_Package LT_Package_LISTTALK;
extern LT_Package LT_Package_LISTTALK_IMPLEMENTATION;
extern LT_Package LT_Package_LISTTALK_USER;
extern LT_Package LT_Package_KEYWORD;

#define LT_PACKAGE_LISTTALK (&LT_Package_LISTTALK)
#define LT_PACKAGE_LISTTALK_IMPLEMENTATION (&LT_Package_LISTTALK_IMPLEMENTATION)
#define LT_PACKAGE_LISTTALK_USER (&LT_Package_LISTTALK_USER)
#define LT_PACKAGE_KEYWORD (&LT_Package_KEYWORD)

LT__END_DECLS

#endif
