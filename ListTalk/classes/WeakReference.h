/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__WeakReference__
#define H__ListTalk__WeakReference__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/value.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_WeakReference);

LT_WeakReference* LT_WeakReference_new(LT_Value value);
int LT_WeakReference_alive_p(LT_WeakReference* reference);
LT_Value LT_WeakReference_value(LT_WeakReference* reference);
void LT_WeakReference_setValue(LT_WeakReference* reference, LT_Value value);

LT__END_DECLS

#endif
