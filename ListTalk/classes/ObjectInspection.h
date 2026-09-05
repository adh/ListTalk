/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__ObjectInspection__
#define H__ListTalk__ObjectInspection__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_ObjectInspection);

struct LT_ObjectInspection_s {
    LT_Object base;
    LT_Value name;
    LT_Value description;
    LT_Value slots;
    LT_Value contents_label;
    LT_Value contents;
};

LT_Value LT_ObjectInspection_new(LT_Value name,
                                 LT_Value description,
                                 LT_Value slots,
                                 LT_Value contents_label,
                                 LT_Value contents);
LT_Value LT_ObjectInspection_name(LT_ObjectInspection* inspection);
LT_Value LT_ObjectInspection_description(LT_ObjectInspection* inspection);
LT_Value LT_ObjectInspection_slots(LT_ObjectInspection* inspection);
LT_Value LT_ObjectInspection_contents_label(LT_ObjectInspection* inspection);
LT_Value LT_ObjectInspection_contents(LT_ObjectInspection* inspection);

LT__END_DECLS

#endif
