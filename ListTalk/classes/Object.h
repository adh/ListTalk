/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Object__
#define H__ListTalk__Object__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/vm/Class.h>

LT__BEGIN_DECLS

/* There is no point in having LT_Object_from_object(). 
 * LT_Object and struct LT_Object_s are defined in value.h.
 */

extern LT_Class LT_Object_class;
extern LT_Class LT_Object_class_class;

LT_Value LT_Object_slot_ref(LT_Value object, LT_Value slot_name);
LT_Value LT_Object_slot_set(LT_Value object, LT_Value slot_name, LT_Value value);

LT__END_DECLS

#endif
