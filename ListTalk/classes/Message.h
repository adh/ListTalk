/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Message__
#define H__ListTalk__Message__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Message);

LT_Value LT_Message_new(
    LT_Value selector,
    LT_Value receiver,
    LT_Value arguments
);
LT_Value LT_Message_selector(LT_Message* message);
LT_Value LT_Message_receiver(LT_Message* message);
LT_Value LT_Message_arguments(LT_Message* message);

LT__END_DECLS

#endif
