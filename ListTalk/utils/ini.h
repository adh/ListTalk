/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__utils__ini__
#define H__ListTalk__utils__ini__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/vm/value.h>

#include <stddef.h>

LT__BEGIN_DECLS

typedef struct LT_INI LT_INI;

#define LT_INI_DUPLICATE_LAST_WINS 0
#define LT_INI_DUPLICATE_FIRST_WINS 1
#define LT_INI_DUPLICATE_ERROR 2

#define LT_INI_ALLOW_GLOBAL_KEYS 0x01
#define LT_INI_ALLOW_EMPTY_VALUES 0x02

LT_INI* LT_INI_parseBytes(
    const char* source_name,
    const char* bytes,
    size_t length,
    unsigned int flags,
    int duplicate_policy
);

LT_INI* LT_INI_parseString(
    const char* source_name,
    LT_String* string,
    unsigned int flags,
    int duplicate_policy
);

LT_INI* LT_INI_parseFile(
    const char* path,
    unsigned int flags,
    int duplicate_policy
);

int LT_INI_at(
    LT_INI* ini,
    const char* section,
    const char* key,
    const char** value_out
);

LT_Value LT_INI_asDictionary(LT_INI* ini);
LT_Value LT_INI_sectionNames(LT_INI* ini);
LT_Value LT_INI_sectionAsDictionary(LT_INI* ini, const char* section);

LT__END_DECLS

#endif
