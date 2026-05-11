/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/IdentityDictionary.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/WeakKeyIdentityDictionary.h>
#include <ListTalk/classes/WeakValueIdentityDictionary.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/weak.h>
#include <ListTalk/utils.h>

#include <stdint.h>
#include <string.h>

struct LT_IdentityDictionary_Entry {
    LT_Value value;
    LT_WeakValue weak_value;
};

struct LT_IdentityDictionary_s {
    LT_Object base;
    LT_InlineHash table;
};

struct LT_WeakKeyIdentityDictionary_s {
    LT_Object base;
    LT_InlineHash table;
};

struct LT_WeakValueIdentityDictionary_s {
    LT_Object base;
    LT_InlineHash table;
};

static int dictionary_weak_key_p(LT_IdentityDictionary* dictionary){
    return dictionary->base.klass == &LT_WeakKeyIdentityDictionary_class;
}

static int dictionary_weak_value_p(LT_IdentityDictionary* dictionary){
    return dictionary->base.klass == &LT_WeakValueIdentityDictionary_class;
}

static LT_IdentityDictionary* identity_dictionary_from_value(LT_Value value){
    if (!LT_Value_is_instance_of(
        value,
        (LT_Value)(uintptr_t)&LT_IdentityDictionary_class
    )){
        LT_type_error(value, &LT_IdentityDictionary_class);
    }
    return (LT_IdentityDictionary*)LT_VALUE_POINTER_VALUE(value);
}

static int dictionary_entry_key(LT_IdentityDictionary* dictionary,
                                LT_InlineHash_Entry* entry,
                                LT_Value* key_out){
    if (dictionary_weak_key_p(dictionary)){
        LT_WeakValue* weak = (LT_WeakValue*)entry->key;

        if (!LT_weak_is_alive(*weak)){
            return 0;
        }
        *key_out = LT_weak_unbox(*weak);
        return 1;
    }

    *key_out = (LT_Value)(uintptr_t)entry->key;
    return 1;
}

static int dictionary_entry_value(LT_IdentityDictionary* dictionary,
                                  struct LT_IdentityDictionary_Entry* entry,
                                  LT_Value* value_out){
    if (dictionary_weak_value_p(dictionary)){
        if (!LT_weak_is_alive(entry->weak_value)){
            return 0;
        }
        *value_out = LT_weak_unbox(entry->weak_value);
        return 1;
    }

    *value_out = entry->value;
    return 1;
}

static int dictionary_entry_alive(LT_IdentityDictionary* dictionary,
                                  LT_InlineHash_Entry* table_entry){
    struct LT_IdentityDictionary_Entry* entry =
        (struct LT_IdentityDictionary_Entry*)table_entry->value;
    LT_Value ignored;

    return dictionary_entry_key(dictionary, table_entry, &ignored)
        && dictionary_entry_value(dictionary, entry, &ignored);
}

static void IdentityDictionary_debugPrintOn(LT_Value obj, FILE* stream){
    LT_IdentityDictionary* dictionary = LT_IdentityDictionary_from_value(obj);
    fprintf(stream, "#<IdentityDictionary %p size=%zu>",
        (void*)dictionary,
        LT_InlineHash_count(&dictionary->table)
    );
}

static void WeakKeyIdentityDictionary_debugPrintOn(LT_Value obj, FILE* stream){
    LT_IdentityDictionary* dictionary = (LT_IdentityDictionary*)LT_VALUE_POINTER_VALUE(obj);
    fprintf(stream, "#<WeakKeyIdentityDictionary %p size=%zu>",
        (void*)dictionary,
        LT_InlineHash_count(&dictionary->table)
    );
}

static void WeakValueIdentityDictionary_debugPrintOn(LT_Value obj, FILE* stream){
    LT_IdentityDictionary* dictionary = (LT_IdentityDictionary*)LT_VALUE_POINTER_VALUE(obj);
    fprintf(stream, "#<WeakValueIdentityDictionary %p size=%zu>",
        (void*)dictionary,
        LT_InlineHash_count(&dictionary->table)
    );
}

LT_DEFINE_PRIMITIVE(
    identity_dictionary_class_method_new,
    "IdentityDictionary class>>new",
    "(self)",
    "Return a new empty identity dictionary."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_IdentityDictionary_class){
        LT_error("new class method is only supported on IdentityDictionary");
    }
    return (LT_Value)(uintptr_t)LT_IdentityDictionary_new();
}

LT_DEFINE_PRIMITIVE(
    identity_dictionary_class_method_new_from_alist,
    "IdentityDictionary class>>newFromAList:",
    "(self alist)",
    "Return an identity dictionary initialized from an association list."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value alist;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, alist);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_IdentityDictionary_class){
        LT_error(
            "newFromAList: class method is only supported on IdentityDictionary"
        );
    }
    return (LT_Value)(uintptr_t)LT_IdentityDictionary_newFromAList(alist);
}

LT_DEFINE_PRIMITIVE(
    weak_key_identity_dictionary_class_method_new,
    "WeakKeyIdentityDictionary class>>new",
    "(self)",
    "Return a new empty weak-key identity dictionary."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_WeakKeyIdentityDictionary_class){
        LT_error("new class method is only supported on WeakKeyIdentityDictionary");
    }
    return (LT_Value)(uintptr_t)LT_WeakKeyIdentityDictionary_new();
}

LT_DEFINE_PRIMITIVE(
    weak_key_identity_dictionary_class_method_new_from_alist,
    "WeakKeyIdentityDictionary class>>newFromAList:",
    "(self alist)",
    "Return a weak-key identity dictionary initialized from an association list."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value alist;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, alist);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_WeakKeyIdentityDictionary_class){
        LT_error(
            "newFromAList: class method is only supported on WeakKeyIdentityDictionary"
        );
    }
    return (LT_Value)(uintptr_t)LT_WeakKeyIdentityDictionary_newFromAList(alist);
}

LT_DEFINE_PRIMITIVE(
    weak_value_identity_dictionary_class_method_new,
    "WeakValueIdentityDictionary class>>new",
    "(self)",
    "Return a new empty weak-value identity dictionary."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_WeakValueIdentityDictionary_class){
        LT_error("new class method is only supported on WeakValueIdentityDictionary");
    }
    return (LT_Value)(uintptr_t)LT_WeakValueIdentityDictionary_new();
}

LT_DEFINE_PRIMITIVE(
    weak_value_identity_dictionary_class_method_new_from_alist,
    "WeakValueIdentityDictionary class>>newFromAList:",
    "(self alist)",
    "Return a weak-value identity dictionary initialized from an association list."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value alist;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, alist);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_WeakValueIdentityDictionary_class){
        LT_error(
            "newFromAList: class method is only supported on WeakValueIdentityDictionary"
        );
    }
    return (LT_Value)(uintptr_t)LT_WeakValueIdentityDictionary_newFromAList(alist);
}

LT_DEFINE_PRIMITIVE(
    identity_dictionary_method_size,
    "IdentityDictionary>>size",
    "(self)",
    "Return dictionary size."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Number_smallinteger_from_size(
        LT_IdentityDictionary_size(identity_dictionary_from_value(self)),
        "IdentityDictionary size does not fit fixnum"
    );
}

LT_DEFINE_PRIMITIVE(
    identity_dictionary_method_at,
    "IdentityDictionary>>at:",
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
    if (!LT_IdentityDictionary_at(identity_dictionary_from_value(self), key, &value)){
        return LT_NIL;
    }
    return value;
}

LT_DEFINE_PRIMITIVE(
    identity_dictionary_method_at_put,
    "IdentityDictionary>>at:put:",
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
    LT_IdentityDictionary_atPut(identity_dictionary_from_value(self), key, value);
    return value;
}

LT_DEFINE_PRIMITIVE(
    identity_dictionary_method_contains,
    "IdentityDictionary>>contains?:",
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
    return LT_IdentityDictionary_at(identity_dictionary_from_value(self), key, NULL)
        ? LT_TRUE
        : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    identity_dictionary_method_remove,
    "IdentityDictionary>>remove:",
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
    if (!LT_IdentityDictionary_remove(
        identity_dictionary_from_value(self),
        key,
        &value
    )){
        return LT_NIL;
    }
    return value;
}

static LT_Method_Descriptor IdentityDictionary_methods[] = {
    {"size", &identity_dictionary_method_size},
    {"at:", &identity_dictionary_method_at},
    {"at:put:", &identity_dictionary_method_at_put},
    {"contains?:", &identity_dictionary_method_contains},
    {"remove:", &identity_dictionary_method_remove},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor IdentityDictionary_class_methods[] = {
    {"new", &identity_dictionary_class_method_new},
    {"newFromAList:", &identity_dictionary_class_method_new_from_alist},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor WeakKeyIdentityDictionary_class_methods[] = {
    {"new", &weak_key_identity_dictionary_class_method_new},
    {"newFromAList:", &weak_key_identity_dictionary_class_method_new_from_alist},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor WeakValueIdentityDictionary_class_methods[] = {
    {"new", &weak_value_identity_dictionary_class_method_new},
    {"newFromAList:", &weak_value_identity_dictionary_class_method_new_from_alist},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_IdentityDictionary) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "IdentityDictionary",
    .instance_size = sizeof(LT_IdentityDictionary),
    .debugPrintOn = IdentityDictionary_debugPrintOn,
    .methods = IdentityDictionary_methods,
    .class_methods = IdentityDictionary_class_methods,
};

LT_DEFINE_CLASS(LT_WeakKeyIdentityDictionary) {
    .superclass = &LT_IdentityDictionary_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "WeakKeyIdentityDictionary",
    .instance_size = sizeof(LT_WeakKeyIdentityDictionary),
    .debugPrintOn = WeakKeyIdentityDictionary_debugPrintOn,
    .class_methods = WeakKeyIdentityDictionary_class_methods,
};

LT_DEFINE_CLASS(LT_WeakValueIdentityDictionary) {
    .superclass = &LT_IdentityDictionary_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "WeakValueIdentityDictionary",
    .instance_size = sizeof(LT_WeakValueIdentityDictionary),
    .debugPrintOn = WeakValueIdentityDictionary_debugPrintOn,
    .class_methods = WeakValueIdentityDictionary_class_methods,
};

static void dictionary_grow_table(LT_IdentityDictionary* dictionary){
    LT_InlineHash* table = &dictionary->table;
    LT_InlineHash_Entry** grown_vector;
    size_t grown_size;
    size_t grown_count = 0;
    size_t i;

    grown_size = (table->mask + 1) << 1;
    grown_vector = GC_MALLOC(sizeof(LT_InlineHash_Entry*) * grown_size);
    memset(grown_vector, 0, sizeof(LT_InlineHash_Entry*) * grown_size);

    for (i = 0; i < table->mask + 1; i++){
        LT_InlineHash_Entry* entry = table->vector[i];

        while (entry != NULL){
            LT_InlineHash_Entry* next = entry->next;

            if (dictionary_entry_alive(dictionary, entry)){
                size_t index = entry->hash & (grown_size - 1);

                entry->next = grown_vector[index];
                grown_vector[index] = entry;
                grown_count++;
            }
            entry = next;
        }
    }

    table->vector = grown_vector;
    table->mask = grown_size - 1;
    table->count = grown_count;
}

static void dictionary_entry_set_key(LT_IdentityDictionary* dictionary,
                                     LT_InlineHash_Entry* table_entry,
                                     LT_Value key){
    if (dictionary_weak_key_p(dictionary)){
        LT_WeakValue* weak = GC_NEW(LT_WeakValue);

        LT_weak_box(weak, key);
        table_entry->key = weak;
        return;
    }

    table_entry->key = (void*)(uintptr_t)key;
}

static void dictionary_entry_set_value(LT_IdentityDictionary* dictionary,
                                       struct LT_IdentityDictionary_Entry* entry,
                                       LT_Value value){
    if (dictionary_weak_value_p(dictionary)){
        LT_weak_box(&entry->weak_value, value);
        entry->value = LT_NIL;
        return;
    }

    entry->value = value;
    entry->weak_value.masked_value = 0;
}

static LT_InlineHash_Entry* dictionary_find_entry(
    LT_IdentityDictionary* dictionary,
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
        LT_Value entry_key;

        if (entry->hash == hash
            && dictionary_entry_key(dictionary, entry, &entry_key)
            && entry_key == key){
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

static LT_IdentityDictionary* identity_dictionary_new_with_class(LT_Class* klass){
    LT_IdentityDictionary* dictionary = (LT_IdentityDictionary*)LT_Class_alloc(klass);
    LT_InlineHash_init(&dictionary->table);
    return dictionary;
}

LT_IdentityDictionary* LT_IdentityDictionary_new(void){
    return identity_dictionary_new_with_class(&LT_IdentityDictionary_class);
}

static LT_IdentityDictionary* identity_dictionary_new_from_alist_with_class(
    LT_Class* klass,
    LT_Value alist
){
    LT_IdentityDictionary* dictionary = identity_dictionary_new_with_class(klass);

    while (alist != LT_NIL){
        LT_Value entry;

        if (!LT_Pair_p(alist)){
            LT_error("IdentityDictionary newFromAList: expects proper list");
        }

        entry = LT_car(alist);
        if (!LT_Pair_p(entry)){
            LT_error("IdentityDictionary newFromAList: expects list of pairs");
        }

        LT_IdentityDictionary_atPut(dictionary, LT_car(entry), LT_cdr(entry));
        alist = LT_cdr(alist);
    }

    return dictionary;
}

LT_IdentityDictionary* LT_IdentityDictionary_newFromAList(LT_Value alist){
    return identity_dictionary_new_from_alist_with_class(
        &LT_IdentityDictionary_class,
        alist
    );
}

LT_WeakKeyIdentityDictionary* LT_WeakKeyIdentityDictionary_new(void){
    return (LT_WeakKeyIdentityDictionary*)identity_dictionary_new_with_class(
        &LT_WeakKeyIdentityDictionary_class
    );
}

LT_WeakKeyIdentityDictionary* LT_WeakKeyIdentityDictionary_newFromAList(
    LT_Value alist
){
    return (LT_WeakKeyIdentityDictionary*)
        identity_dictionary_new_from_alist_with_class(
            &LT_WeakKeyIdentityDictionary_class,
            alist
        );
}

LT_WeakValueIdentityDictionary* LT_WeakValueIdentityDictionary_new(void){
    return (LT_WeakValueIdentityDictionary*)identity_dictionary_new_with_class(
        &LT_WeakValueIdentityDictionary_class
    );
}

LT_WeakValueIdentityDictionary* LT_WeakValueIdentityDictionary_newFromAList(
    LT_Value alist
){
    return (LT_WeakValueIdentityDictionary*)
        identity_dictionary_new_from_alist_with_class(
            &LT_WeakValueIdentityDictionary_class,
            alist
        );
}

size_t LT_IdentityDictionary_size(LT_IdentityDictionary* dictionary){
    return LT_InlineHash_count(&dictionary->table);
}

void LT_IdentityDictionary_atPut(
    LT_IdentityDictionary* dictionary,
    LT_Value key,
    LT_Value value
){
    LT_InlineHash* table = &dictionary->table;
    size_t hash = LT_pointer_hash((void*)(uintptr_t)key);
    LT_InlineHash_Entry* table_entry;
    struct LT_IdentityDictionary_Entry* entry;

    table_entry = dictionary_find_entry(dictionary, key, hash, NULL, NULL);
    if (table_entry != NULL){
        entry = (struct LT_IdentityDictionary_Entry*)table_entry->value;
        dictionary_entry_set_value(dictionary, entry, value);
        return;
    }

    if (table->count > table->mask){
        dictionary_grow_table(dictionary);
    }

    table_entry = GC_NEW(LT_InlineHash_Entry);
    entry = GC_NEW(struct LT_IdentityDictionary_Entry);
    table_entry->hash = hash;
    dictionary_entry_set_key(dictionary, table_entry, key);
    dictionary_entry_set_value(dictionary, entry, value);
    table_entry->value = entry;
    table_entry->next = table->vector[hash & table->mask];
    table->vector[hash & table->mask] = table_entry;
    table->count++;
}

int LT_IdentityDictionary_at(
    LT_IdentityDictionary* dictionary,
    LT_Value key,
    LT_Value* value_out
){
    size_t hash = LT_pointer_hash((void*)(uintptr_t)key);
    LT_InlineHash_Entry* table_entry;
    struct LT_IdentityDictionary_Entry* entry;
    LT_Value value;

    table_entry = dictionary_find_entry(dictionary, key, hash, NULL, NULL);
    if (table_entry == NULL){
        return 0;
    }

    entry = (struct LT_IdentityDictionary_Entry*)table_entry->value;
    if (!dictionary_entry_value(dictionary, entry, &value)){
        return 0;
    }

    if (value_out != NULL){
        *value_out = value;
    }
    return 1;
}

int LT_IdentityDictionary_remove(
    LT_IdentityDictionary* dictionary,
    LT_Value key,
    LT_Value* value_out
){
    LT_InlineHash* table = &dictionary->table;
    size_t hash = LT_pointer_hash((void*)(uintptr_t)key);
    LT_InlineHash_Entry* previous = NULL;
    size_t index = 0;
    LT_InlineHash_Entry* table_entry;
    struct LT_IdentityDictionary_Entry* entry;

    table_entry = dictionary_find_entry(
        dictionary,
        key,
        hash,
        &previous,
        &index
    );
    if (table_entry == NULL){
        return 0;
    }

    if (previous == NULL){
        table->vector[index] = table_entry->next;
    } else {
        previous->next = table_entry->next;
    }

    table->count--;
    entry = (struct LT_IdentityDictionary_Entry*)table_entry->value;
    if (value_out != NULL
        && !dictionary_entry_value(dictionary, entry, value_out)){
        *value_out = LT_NIL;
    }
    return 1;
}

void LT_IdentityDictionary_keys_do(
    LT_IdentityDictionary* dictionary,
    void (*callback)(LT_Value key, void* baton),
    void* baton
){
    LT_InlineHash* table = &dictionary->table;
    size_t i;

    for (i = 0; i < table->mask + 1; i++){
        LT_InlineHash_Entry* table_entry = table->vector[i];

        while (table_entry != NULL){
            LT_Value key;

            if (dictionary_entry_key(dictionary, table_entry, &key)
                && dictionary_entry_alive(dictionary, table_entry)){
                callback(key, baton);
            }
            table_entry = table_entry->next;
        }
    }
}
