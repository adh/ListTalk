/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__RegularExpression__
#define H__ListTalk__RegularExpression__

#include <ListTalk/classes/String.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_RegularExpression);

LT_RegularExpression* LT_RegularExpression_new(LT_String* pattern);
LT_String* LT_RegularExpression_pattern(LT_RegularExpression* expression);
LT_Value LT_RegularExpression_match(
    LT_RegularExpression* expression,
    LT_String* subject
);
LT_String* LT_RegularExpression_substitute(
    LT_RegularExpression* expression,
    LT_String* subject,
    LT_String* replacement
);

LT__END_DECLS

#endif
