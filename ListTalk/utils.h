/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__utils__
#define H__ListTalk__utils__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/vm/value.h>

#include <stdint.h>

LT__BEGIN_DECLS

extern uint32_t LT_fnv_hash(char* string);
extern uint32_t LT_pointer_hash(void* pointer);

extern char* LT_strdup(char*);

typedef struct LT_StringBuilder LT_StringBuilder;

LT_StringBuilder* LT_StringBuilder_new();
void LT_StringBuilder_append_str(LT_StringBuilder* builder, char*str);
void LT_StringBuilder_append_char(LT_StringBuilder* builder, char ch);
char* LT_StringBuilder_value(LT_StringBuilder* builder);
size_t LT_StringBuilder_length(LT_StringBuilder* builder);

typedef struct LT_ListBuilder LT_ListBuilder;

LT_ListBuilder* LT_ListBuilder_new();
void LT_ListBuilder_append(LT_ListBuilder* builder, LT_Value value);
LT_Value LT_ListBuilder_value(LT_ListBuilder* builder);
LT_Value LT_ListBuilder_valueWithRest(LT_ListBuilder* builder, LT_Value rest);
size_t LT_ListBuilder_length(LT_ListBuilder* builder);


typedef struct LT_InlineHash LT_InlineHash;
typedef struct LT_InlineHash_Entry LT_InlineHash_Entry;

struct LT_InlineHash {
    LT_InlineHash_Entry** vector;
    size_t mask;
    size_t count;
};

struct LT_InlineHash_Entry {
    size_t hash;
    void* key;
    void* value;
    LT_InlineHash_Entry* next;
};

void LT_InlineHash_init(LT_InlineHash* h);
size_t LT_InlineHash_count(LT_InlineHash* h);


void LT_StringHash_at_put(LT_InlineHash* sh,
                          char* key, void* value);
void* LT_StringHash_at(LT_InlineHash* sh, char* key);
int LT_StringHash_remove(
    LT_InlineHash* sh,
    char* key,
    void** value_out
);

void LT_PointerHash_at_put(LT_InlineHash* sh,
                           void* key, void* value);
void* LT_PointerHash_at(LT_InlineHash* sh,
                        void* key);
int LT_PointerHash_remove(
    LT_InlineHash* sh,
    void* key,
    void** value_out
);

/* constructors registered here are called from LT_init() after symbols are
 * setup and native class initialization. So the VM is guaranteed to not be
 * in some weird state. */
void LT_register_constructor(void (*ctor)(void));

#define LT_REGISTER_CONSTRUCTOR(ctor) \
    void __attribute__((constructor)) LT___register_constructor_##ctor(void){ \
        LT_register_constructor(ctor); \
    }

#define LT_REGISTER_INITIALIZER(init) \
    void __attribute__((constructor)) LT___register_initializer_##init(void){ \
        LT_register_constructor(init); \
    }

LT__END_DECLS

#endif
