/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__EmptyIterator__
#define H__ListTalk__EmptyIterator__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_EmptyIterator);

LT_EmptyIterator* LT_EmptyIterator_instance(void);

LT__END_DECLS

#endif
