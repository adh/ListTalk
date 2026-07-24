/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__metadata__
#define H__ListTalk__metadata__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/vm/value.h>

LT__BEGIN_DECLS

LT_Value LT_parse_lambda_list_metadata(char* arguments_text);

LT__END_DECLS

#endif
