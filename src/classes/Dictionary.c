/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Dictionary.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/ListTalk.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>
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

static LT_Dictionary* dictionary_from_value(LT_Value value){
    if (!LT_Value_is_instance_of(
        value,
        (LT_Value)(uintptr_t)&LT_Dictionary_class
    )){
        LT_type_error(value, &LT_Dictionary_class);
    }
    return (LT_Dictionary*)LT_VALUE_POINTER_VALUE(value);
}

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

LT_Dictionary* LT_Dictionary_newFromAList(LT_Value alist){
    LT_Dictionary* dictionary = LT_Dictionary_new();

    while (alist != LT_NIL){
        LT_Value entry;

        if (!LT_Pair_p(alist)){
            LT_error("Dictionary newFromAList: expects proper list");
        }

        entry = LT_car(alist);
        if (!LT_Pair_p(entry)){
            LT_error("Dictionary newFromAList: expects list of pairs");
        }

        LT_Dictionary_atPut(dictionary, LT_car(entry), LT_cdr(entry));
        alist = LT_cdr(alist);
    }

    return dictionary;
}

size_t LT_Dictionary_size(LT_Dictionary* dictionary){
    return LT_InlineHash_count(&dictionary->table);
}

static LT_Value dictionary_apply2(LT_Value callable, LT_Value key, LT_Value value){
    return LT_apply(
        callable,
        LT_cons(key, LT_cons(value, LT_NIL)),
        LT_NIL,
        LT_NIL,
        NULL
    );
}

LT_Value LT_Dictionary_asAList(LT_Dictionary* dictionary){
    LT_InlineHash* table = &dictionary->table;
    LT_ListBuilder* builder = LT_ListBuilder_new();
    size_t i;

    for (i = 0; i < table->mask + 1; i++){
        LT_InlineHash_Entry* entry = table->vector[i];

        while (entry != NULL){
            LT_ListBuilder_append(
                builder,
                LT_cons(
                    (LT_Value)(uintptr_t)entry->key,
                    (LT_Value)(uintptr_t)entry->value
                )
            );
            entry = entry->next;
        }
    }

    return LT_ListBuilder_value(builder);
}

void LT_Dictionary_for_each(LT_Dictionary* dictionary, LT_Value callable){
    LT_InlineHash* table = &dictionary->table;
    size_t i;

    for (i = 0; i < table->mask + 1; i++){
        LT_InlineHash_Entry* entry = table->vector[i];

        while (entry != NULL){
            (void)dictionary_apply2(
                callable,
                (LT_Value)(uintptr_t)entry->key,
                (LT_Value)(uintptr_t)entry->value
            );
            entry = entry->next;
        }
    }
}

LT_Value LT_Dictionary_map(LT_Dictionary* dictionary, LT_Value callable){
    LT_InlineHash* table = &dictionary->table;
    LT_ListBuilder* builder = LT_ListBuilder_new();
    size_t i;

    for (i = 0; i < table->mask + 1; i++){
        LT_InlineHash_Entry* entry = table->vector[i];

        while (entry != NULL){
            LT_ListBuilder_append(
                builder,
                dictionary_apply2(
                    callable,
                    (LT_Value)(uintptr_t)entry->key,
                    (LT_Value)(uintptr_t)entry->value
                )
            );
            entry = entry->next;
        }
    }

    return LT_ListBuilder_value(builder);
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

LT_DEFINE_PRIMITIVE(
    dictionary_class_method_new,
    "Dictionary class>>new",
    "(self)",
    "Return a new empty dictionary."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_Dictionary_class){
        LT_error("new class method is only supported on Dictionary");
    }
    return (LT_Value)(uintptr_t)LT_Dictionary_new();
}

LT_DEFINE_PRIMITIVE(
    dictionary_class_method_new_from_alist,
    "Dictionary class>>newFromAList:",
    "(self alist)",
    "Return a dictionary initialized from an association list."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value alist;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, alist);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_Dictionary_class){
        LT_error("newFromAList: class method is only supported on Dictionary");
    }
    return (LT_Value)(uintptr_t)LT_Dictionary_newFromAList(alist);
}

LT_DEFINE_PRIMITIVE(
    dictionary_method_size,
    "Dictionary>>size",
    "(self)",
    "Return dictionary size."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Number_smallinteger_from_size(
        LT_Dictionary_size(dictionary_from_value(self)),
        "Dictionary size does not fit fixnum"
    );
}

LT_DEFINE_PRIMITIVE(
    dictionary_method_at,
    "Dictionary>>at:",
    "(self key)",
    "Return value for key, or nil when key is absent."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value key;
    LT_Value value = LT_NIL;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, key);
    LT_ARG_END(cursor);
    if (!LT_Dictionary_at(dictionary_from_value(self), key, &value)){
        return LT_NIL;
    }
    return value;
}

LT_DEFINE_PRIMITIVE(
    dictionary_method_as_alist,
    "Dictionary>>asAList",
    "(self)",
    "Return dictionary entries as an association list."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Dictionary_asAList(dictionary_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    dictionary_method_at_put,
    "Dictionary>>at:put:",
    "(self key value)",
    "Set value for key and return value."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value key;
    LT_Value value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, key);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    LT_Dictionary_atPut(dictionary_from_value(self), key, value);
    return value;
}

LT_DEFINE_PRIMITIVE(
    dictionary_method_contains,
    "Dictionary>>contains?:",
    "(self key)",
    "Return true when dictionary contains key."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value key;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, key);
    LT_ARG_END(cursor);
    return LT_Dictionary_at(dictionary_from_value(self), key, NULL)
        ? LT_TRUE
        : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    dictionary_method_for_each,
    "Dictionary>>forEach:",
    "(self callable)",
    "Apply callable to each key and value, and return nil."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    LT_Dictionary_for_each(dictionary_from_value(self), callable);
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE(
    dictionary_method_map,
    "Dictionary>>map:",
    "(self callable)",
    "Return list of callable results for each key and value."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    return LT_Dictionary_map(dictionary_from_value(self), callable);
}

LT_DEFINE_PRIMITIVE(
    dictionary_method_remove,
    "Dictionary>>remove:",
    "(self key)",
    "Remove key and return its value, or nil when key is absent."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value key;
    LT_Value value = LT_NIL;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, key);
    LT_ARG_END(cursor);
    if (!LT_Dictionary_remove(dictionary_from_value(self), key, &value)){
        return LT_NIL;
    }
    return value;
}

static LT_Method_Descriptor Dictionary_methods[] = {
    {"size", &dictionary_method_size},
    {"at:", &dictionary_method_at},
    {"asAList", &dictionary_method_as_alist},
    {"at:put:", &dictionary_method_at_put},
    {"contains?:", &dictionary_method_contains},
    {"forEach:", &dictionary_method_for_each},
    {"map:", &dictionary_method_map},
    {"remove:", &dictionary_method_remove},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor Dictionary_class_methods[] = {
    {"new", &dictionary_class_method_new},
    {"newFromAList:", &dictionary_class_method_new_from_alist},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Dictionary) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Dictionary",
    .instance_size = sizeof(LT_Dictionary),
    .debugPrintOn = Dictionary_debugPrintOn,
    .methods = Dictionary_methods,
    .class_methods = Dictionary_class_methods,
};

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
