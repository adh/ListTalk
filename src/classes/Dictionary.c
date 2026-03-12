/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Dictionary.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/utils.h>

#include <stdint.h>
#include <string.h>

struct LT_Dictionary_s {
    LT_Object base;
    LT_InlineHash table;
};

static void Dictionary_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Dictionary* dictionary = LT_Dictionary_from_value(obj);
    fprintf(stream, "#<Dictionary %p size=%zu>",
        (void*)dictionary,
        LT_InlineHash_count(&dictionary->table)
    );
}

LT_DEFINE_CLASS(LT_Dictionary) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Dictionary",
    .instance_size = sizeof(LT_Dictionary),
    .debugPrintOn = Dictionary_debugPrintOn,
};

static void dictionary_grow_table(LT_Dictionary* dictionary){
    LT_InlineHash* table = &dictionary->table;
    LT_InlineHash_Entry** grown_vector;
    size_t grown_size;
    size_t i;

    grown_size = (table->mask + 1) << 1;
    grown_vector = GC_MALLOC(sizeof(LT_InlineHash_Entry*) * grown_size);
    memset(grown_vector, 0, sizeof(LT_InlineHash_Entry*) * grown_size);

    for (i = 0; i < table->mask + 1; i++){
        LT_InlineHash_Entry* entry = table->vector[i];

        while (entry != NULL){
            LT_InlineHash_Entry* next = entry->next;
            size_t index = entry->hash & (grown_size - 1);

            entry->next = grown_vector[index];
            grown_vector[index] = entry;
            entry = next;
        }
    }

    table->vector = grown_vector;
    table->mask = grown_size - 1;
}

static LT_InlineHash_Entry* dictionary_find_entry(
    LT_Dictionary* dictionary,
    LT_Value key,
    size_t hash,
    LT_InlineHash_Entry** previous_out,
    size_t* index_out
){
    LT_InlineHash* table = &dictionary->table;
    size_t index = hash & table->mask;
    LT_InlineHash_Entry* previous = NULL;
    LT_InlineHash_Entry* entry = table->vector[index];

    while (entry != NULL){
        LT_Value entry_key = (LT_Value)(uintptr_t)entry->key;

        if (entry->hash == hash && LT_Value_equal_p(entry_key, key)){
            if (previous_out != NULL){
                *previous_out = previous;
            }
            if (index_out != NULL){
                *index_out = index;
            }
            return entry;
        }

        previous = entry;
        entry = entry->next;
    }

    if (previous_out != NULL){
        *previous_out = previous;
    }
    if (index_out != NULL){
        *index_out = index;
    }
    return NULL;
}

LT_Dictionary* LT_Dictionary_new(void){
    LT_Dictionary* dictionary = LT_Class_ALLOC(LT_Dictionary);
    LT_InlineHash_init(&dictionary->table);
    return dictionary;
}

size_t LT_Dictionary_size(LT_Dictionary* dictionary){
    return LT_InlineHash_count(&dictionary->table);
}

void LT_Dictionary_atPut(
    LT_Dictionary* dictionary,
    LT_Value key,
    LT_Value value
){
    LT_InlineHash* table = &dictionary->table;
    size_t hash = LT_Value_hash(key);
    LT_InlineHash_Entry* entry = dictionary_find_entry(
        dictionary,
        key,
        hash,
        NULL,
        NULL
    );

    if (entry != NULL){
        entry->value = (void*)(uintptr_t)value;
        return;
    }

    if (table->count > table->mask){
        dictionary_grow_table(dictionary);
    }

    entry = GC_NEW(LT_InlineHash_Entry);
    entry->hash = hash;
    entry->key = (void*)(uintptr_t)key;
    entry->value = (void*)(uintptr_t)value;
    entry->next = table->vector[hash & table->mask];
    table->vector[hash & table->mask] = entry;
    table->count++;
}

int LT_Dictionary_at(
    LT_Dictionary* dictionary,
    LT_Value key,
    LT_Value* value_out
){
    size_t hash = LT_Value_hash(key);
    LT_InlineHash_Entry* entry = dictionary_find_entry(
        dictionary,
        key,
        hash,
        NULL,
        NULL
    );

    if (entry == NULL){
        return 0;
    }

    if (value_out != NULL){
        *value_out = (LT_Value)(uintptr_t)entry->value;
    }
    return 1;
}

int LT_Dictionary_remove(
    LT_Dictionary* dictionary,
    LT_Value key,
    LT_Value* value_out
){
    LT_InlineHash* table = &dictionary->table;
    size_t hash = LT_Value_hash(key);
    LT_InlineHash_Entry* previous = NULL;
    size_t index = 0;
    LT_InlineHash_Entry* entry = dictionary_find_entry(
        dictionary,
        key,
        hash,
        &previous,
        &index
    );

    if (entry == NULL){
        return 0;
    }

    if (previous == NULL){
        table->vector[index] = entry->next;
    } else {
        previous->next = entry->next;
    }

    table->count--;
    if (value_out != NULL){
        *value_out = (LT_Value)(uintptr_t)entry->value;
    }
    return 1;
}
