/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Character__
#define H__ListTalk__Character__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/vm/error.h>

LT__BEGIN_DECLS

typedef struct LT_Character_s LT_Character;

extern LT_Class LT_Character_class;
extern LT_Class LT_Character_class_class;

static inline int LT_Character_p(LT_Value value){
    return LT_VALUE_IS_IMMEDIATE(value)
        && LT_VALUE_IMMEDIATE_TAG(value) == LT_VALUE_IMMEDIATE_TAG_CHARACTER;
}

#define LT_CHARACTER_CODEPOINT_MAX UINT32_C(0x10ffff)

static inline int LT_Character_codepoint_is_valid(uint32_t codepoint){
    if (codepoint > LT_CHARACTER_CODEPOINT_MAX){
        return 0;
    }
    return !(codepoint >= UINT32_C(0xd800) && codepoint <= UINT32_C(0xdfff));
}

static inline uint32_t LT_Character_value(LT_Value value){
    if (!LT_Character_p(value)){
        LT_type_error(value, &LT_Character_class);
    }
    return (uint32_t)LT_VALUE_IMMEDIATE_VALUE(value);
}

static inline LT_Value LT_Character_new(uint32_t codepoint){
    if (!LT_Character_codepoint_is_valid(codepoint)){
        LT_error("Character code point out of range");
    }
    return LT_VALUE_MAKE_IMMEDIATE(
        LT_VALUE_IMMEDIATE_TAG_CHARACTER,
        (uint64_t)codepoint
    );
}

LT__END_DECLS

#endif
