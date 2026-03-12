/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/IdentityDictionary.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/utils.h>

#include <stdint.h>

struct LT_IdentityDictionary_Entry {
    LT_Value value;
};

struct LT_IdentityDictionary_s {
    LT_Object base;
    LT_InlineHash table;
};

static void IdentityDictionary_debugPrintOn(LT_Value obj, FILE* stream){
    LT_IdentityDictionary* dictionary = LT_IdentityDictionary_from_value(obj);
    fprintf(stream, "#<IdentityDictionary %p size=%zu>",
        (void*)dictionary,
        LT_InlineHash_count(&dictionary->table)
    );
}

LT_DEFINE_CLASS(LT_IdentityDictionary) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "IdentityDictionary",
    .instance_size = sizeof(LT_IdentityDictionary),
    .debugPrintOn = IdentityDictionary_debugPrintOn,
};

static void* identity_dictionary_key(LT_Value value){
    return (void*)(uintptr_t)value;
}

LT_IdentityDictionary* LT_IdentityDictionary_new(void){
    LT_IdentityDictionary* dictionary = LT_Class_ALLOC(LT_IdentityDictionary);
    LT_InlineHash_init(&dictionary->table);
    return dictionary;
}

size_t LT_IdentityDictionary_size(LT_IdentityDictionary* dictionary){
    return LT_InlineHash_count(&dictionary->table);
}

void LT_IdentityDictionary_atPut(
    LT_IdentityDictionary* dictionary,
    LT_Value key,
    LT_Value value
){
    struct LT_IdentityDictionary_Entry* entry;

    entry = LT_PointerHash_at(&dictionary->table, identity_dictionary_key(key));

    if (entry == NULL){
        entry = GC_NEW(struct LT_IdentityDictionary_Entry);
        LT_PointerHash_at_put(
            &dictionary->table,
            identity_dictionary_key(key),
            entry
        );
    }

    entry->value = value;
}

int LT_IdentityDictionary_at(
    LT_IdentityDictionary* dictionary,
    LT_Value key,
    LT_Value* value_out
){
    struct LT_IdentityDictionary_Entry* entry;

    entry = LT_PointerHash_at(&dictionary->table, identity_dictionary_key(key));
    if (entry == NULL){
        return 0;
    }

    if (value_out != NULL){
        *value_out = entry->value;
    }
    return 1;
}

int LT_IdentityDictionary_remove(
    LT_IdentityDictionary* dictionary,
    LT_Value key,
    LT_Value* value_out
){
    struct LT_IdentityDictionary_Entry* entry = NULL;

    if (!LT_PointerHash_remove(
        &dictionary->table,
        identity_dictionary_key(key),
        (void**)&entry
    )){
        return 0;
    }

    if (value_out != NULL){
        *value_out = entry->value;
    }
    return 1;
}
