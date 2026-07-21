/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__MessageQueue__
#define H__ListTalk__MessageQueue__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_MessageQueue);

LT_MessageQueue* LT_MessageQueue_new(size_t capacity);
LT_MessageQueue* LT_MessageQueue_new_named(size_t capacity, LT_Value name);
size_t LT_MessageQueue_capacity(LT_MessageQueue* queue);
size_t LT_MessageQueue_size(LT_MessageQueue* queue);
int LT_MessageQueue_empty_p(LT_MessageQueue* queue);
int LT_MessageQueue_full_p(LT_MessageQueue* queue);
void LT_MessageQueue_put(LT_MessageQueue* queue, LT_Value value);
int LT_MessageQueue_tryPut(LT_MessageQueue* queue, LT_Value value);
LT_Value LT_MessageQueue_get(LT_MessageQueue* queue);
int LT_MessageQueue_tryGet(LT_MessageQueue* queue, LT_Value* value_out);
LT_Value LT_MessageQueue_name(LT_MessageQueue* queue);

LT__END_DECLS

#endif
