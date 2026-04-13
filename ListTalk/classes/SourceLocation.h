/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__SourceLocation__
#define H__ListTalk__SourceLocation__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/vm/error.h>

LT__BEGIN_DECLS

typedef struct LT_SourceLocation_s LT_SourceLocation;

extern LT_Class LT_SourceLocation_class;
extern LT_Class LT_SourceLocation_class_class;

#define LT_SOURCE_LOCATION_LINE_BITS 31
#define LT_SOURCE_LOCATION_COLUMN_BITS 24
#define LT_SOURCE_LOCATION_LINE_OVERFLOW ((UINT32_C(1) << LT_SOURCE_LOCATION_LINE_BITS) - 1)
#define LT_SOURCE_LOCATION_COLUMN_OVERFLOW ((UINT32_C(1) << LT_SOURCE_LOCATION_COLUMN_BITS) - 1)
#define LT_SOURCE_LOCATION_LINE_MAX ((UINT32_C(1) << LT_SOURCE_LOCATION_LINE_BITS) - 2)
#define LT_SOURCE_LOCATION_COLUMN_MAX ((UINT32_C(1) << LT_SOURCE_LOCATION_COLUMN_BITS) - 2)

static inline int LT_SourceLocation_p(LT_Value value){
    return LT_VALUE_IS_IMMEDIATE(value)
        && LT_VALUE_IMMEDIATE_TAG(value) == LT_VALUE_IMMEDIATE_TAG_SOURCE_LOCATION;
}

static inline uint32_t LT_SourceLocation_line(LT_Value value){
    if (!LT_SourceLocation_p(value)){
        LT_type_error(value, &LT_SourceLocation_class);
    }
    return (uint32_t)(LT_VALUE_IMMEDIATE_VALUE(value)
        >> LT_SOURCE_LOCATION_COLUMN_BITS);
}

static inline uint32_t LT_SourceLocation_column(LT_Value value){
    if (!LT_SourceLocation_p(value)){
        LT_type_error(value, &LT_SourceLocation_class);
    }
    return (uint32_t)(LT_VALUE_IMMEDIATE_VALUE(value)
        & ((UINT32_C(1) << LT_SOURCE_LOCATION_COLUMN_BITS) - 1));
}

static inline LT_Value LT_SourceLocation_new(unsigned int line, unsigned int column){
    uint32_t encoded_line = line;
    uint32_t encoded_column = column;

    if (encoded_line > LT_SOURCE_LOCATION_LINE_MAX){
        encoded_line = LT_SOURCE_LOCATION_LINE_OVERFLOW;
    }
    if (encoded_column > LT_SOURCE_LOCATION_COLUMN_MAX){
        encoded_column = LT_SOURCE_LOCATION_COLUMN_OVERFLOW;
    }

    return LT_VALUE_MAKE_IMMEDIATE(
        LT_VALUE_IMMEDIATE_TAG_SOURCE_LOCATION,
        (((uint64_t)encoded_line << LT_SOURCE_LOCATION_COLUMN_BITS)
        | (uint64_t)encoded_column)
    );
}

LT__END_DECLS

#endif
