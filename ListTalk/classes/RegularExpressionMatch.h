/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__RegularExpressionMatch__
#define H__ListTalk__RegularExpressionMatch__

#include <ListTalk/classes/RegularExpression.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_RegularExpressionMatch);

LT_RegularExpression* LT_RegularExpressionMatch_expression(
    LT_RegularExpressionMatch* match
);
LT_String* LT_RegularExpressionMatch_subject(LT_RegularExpressionMatch* match);
size_t LT_RegularExpressionMatch_capture_count(
    LT_RegularExpressionMatch* match
);
LT_Value LT_RegularExpressionMatch_capture(
    LT_RegularExpressionMatch* match,
    size_t index
);
int LT_RegularExpressionMatch_range(
    LT_RegularExpressionMatch* match,
    size_t index,
    size_t* from_out,
    size_t* to_out
);

LT__END_DECLS

#endif
