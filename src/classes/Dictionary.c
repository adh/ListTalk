/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Dictionary.h>
#include <ListTalk/classes/Iterator.h>
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

struct LT_DictionaryIterator_s {
    LT_Object base;
    LT_Dictionary* dictionary;
    size_t bucket_index;
    LT_InlineHash_Entry* entry;
    LT_Value current;
};

static void dictionary_debugPrintOnNamed(
    LT_Value obj,
    FILE* stream,
    const char* name
){
    LT_Dictionary* dictionary = LT_Dictionary_from_value(obj);
    fprintf(stream, "#<%s %p size=%zu>",
        name,
        (void*)dictionary,
        LT_InlineHash_count(&dictionary->table)
    );
}

static void ImmutableDictionary_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Dictionary* dictionary = LT_Dictionary_from_value(obj);
    LT_Value entries = LT_Dictionary_asAList(dictionary);
    int first = 1;

    fputs("#D(", stream);
    while (entries != LT_NIL){
        LT_Value entry = LT_car(entries);

        if (!first){
            fputc(' ', stream);
        }
        LT_Value_debugPrintOn(LT_car(entry), stream);
        fputc(' ', stream);
        LT_Value_debugPrintOn(LT_cdr(entry), stream);
        first = 0;
        entries = LT_cdr(entries);
    }
    fputc(')', stream);
}

static void Dictionary_debugPrintOn(LT_Value obj, FILE* stream){
    dictionary_debugPrintOnNamed(obj, stream, "Dictionary");
}

static void DictionaryIterator_debugPrintOn(LT_Value obj, FILE* stream){
    LT_DictionaryIterator* iterator = LT_DictionaryIterator_from_value(obj);

    fprintf(
        stream,
        "#<DictionaryIterator %p bucket=%zu>",
        (void*)iterator,
        iterator->bucket_index
    );
}

static LT_Dictionary* dictionary_from_value(LT_Value value){
    if (!LT_Dictionary_p(value)){
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

static LT_Dictionary* dictionary_new_with_class(LT_Class* klass){
    LT_Dictionary* dictionary = (LT_Dictionary*)LT_Class_alloc(klass);
    LT_InlineHash_init(&dictionary->table);
    return dictionary;
}

static void dictionary_iterator_find_current(LT_DictionaryIterator* iterator){
    LT_InlineHash* table = &iterator->dictionary->table;

    LT_Mutex_lock(&table->lock);
    while (iterator->entry == NULL && iterator->bucket_index <= table->mask){
        iterator->entry = table->vector[iterator->bucket_index];
        iterator->bucket_index++;
    }

    if (iterator->entry == NULL){
        iterator->current = LT_INVALID;
        LT_Mutex_unlock(&table->lock);
        return;
    }

    iterator->current = LT_cons(
        (LT_Value)(uintptr_t)iterator->entry->key,
        (LT_Value)(uintptr_t)iterator->entry->value
    );
    LT_Mutex_unlock(&table->lock);
}

static void dictionary_iterator_advance(LT_DictionaryIterator* iterator){
    LT_InlineHash* table = &iterator->dictionary->table;

    LT_Mutex_lock(&table->lock);
    if (iterator->entry != NULL){
        iterator->entry = iterator->entry->next;
    }
    LT_Mutex_unlock(&table->lock);
    dictionary_iterator_find_current(iterator);
}

static LT_DictionaryIterator* dictionary_iterator_new(LT_Dictionary* dictionary){
    LT_DictionaryIterator* iterator = LT_Class_ALLOC(LT_DictionaryIterator);

    iterator->dictionary = dictionary;
    iterator->bucket_index = 0;
    iterator->entry = NULL;
    iterator->current = LT_INVALID;
    dictionary_iterator_find_current(iterator);
    return iterator;
}

LT_ImmutableDictionary* LT_ImmutableDictionary_new(void){
    return (LT_ImmutableDictionary*)dictionary_new_with_class(
        &LT_ImmutableDictionary_class
    );
}

LT_Dictionary* LT_Dictionary_new(void){
    return dictionary_new_with_class(&LT_Dictionary_class);
}

static LT_Dictionary* dictionary_new_from_alist_with_class(
    LT_Value alist,
    LT_Class* klass
){
    LT_Dictionary* dictionary = dictionary_new_with_class(klass);

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

LT_ImmutableDictionary* LT_ImmutableDictionary_newFromAList(LT_Value alist){
    return (LT_ImmutableDictionary*)dictionary_new_from_alist_with_class(
        alist,
        &LT_ImmutableDictionary_class
    );
}

LT_Dictionary* LT_Dictionary_newFromAList(LT_Value alist){
    return dictionary_new_from_alist_with_class(alist, &LT_Dictionary_class);
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

    LT_Mutex_lock(&table->lock);
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
    LT_Mutex_unlock(&table->lock);

    return LT_ListBuilder_value(builder);
}

void LT_Dictionary_for_each(LT_Dictionary* dictionary, LT_Value callable){
    LT_Value entries = LT_Dictionary_asAList(dictionary);

    while (entries != LT_NIL){
        LT_Value entry = LT_car(entries);

        (void)dictionary_apply2(callable, LT_car(entry), LT_cdr(entry));
        entries = LT_cdr(entries);
    }
}

LT_Value LT_Dictionary_map(LT_Dictionary* dictionary, LT_Value callable){
    LT_Value entries = LT_Dictionary_asAList(dictionary);
    LT_ListBuilder* builder = LT_ListBuilder_new();

    while (entries != LT_NIL){
        LT_Value entry = LT_car(entries);

        LT_ListBuilder_append(
            builder,
            dictionary_apply2(callable, LT_car(entry), LT_cdr(entry))
        );
        entries = LT_cdr(entries);
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
    LT_InlineHash_Entry* entry;

    LT_Mutex_lock(&table->lock);
    entry = dictionary_find_entry(dictionary, key, hash, NULL, NULL);
    if (entry != NULL){
        entry->value = (void*)(uintptr_t)value;
        LT_Mutex_unlock(&table->lock);
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
    LT_Mutex_unlock(&table->lock);
}

int LT_Dictionary_at(
    LT_Dictionary* dictionary,
    LT_Value key,
    LT_Value* value_out
){
    size_t hash = LT_Value_hash(key);
    LT_InlineHash_Entry* entry;

    LT_Mutex_lock(&dictionary->table.lock);
    entry = dictionary_find_entry(dictionary, key, hash, NULL, NULL);
    if (entry == NULL){
        LT_Mutex_unlock(&dictionary->table.lock);
        return 0;
    }

    if (value_out != NULL){
        *value_out = (LT_Value)(uintptr_t)entry->value;
    }
    LT_Mutex_unlock(&dictionary->table.lock);
    return 1;
}

LT_DEFINE_PRIMITIVE(
    immutable_dictionary_class_method_new,
    "ImmutableDictionary class>>new",
    "(self)",
    "Return a new empty immutable dictionary."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_ImmutableDictionary_class){
        LT_error("new class method is only supported on ImmutableDictionary");
    }
    return (LT_Value)(uintptr_t)LT_ImmutableDictionary_new();
}

LT_DEFINE_PRIMITIVE(
    immutable_dictionary_class_method_new_from_alist,
    "ImmutableDictionary class>>newFromAList:",
    "(self alist)",
    "Return an immutable dictionary initialized from an association list."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value alist;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, alist);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_ImmutableDictionary_class){
        LT_error(
            "newFromAList: class method is only supported on ImmutableDictionary"
        );
    }
    return (LT_Value)(uintptr_t)LT_ImmutableDictionary_newFromAList(alist);
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
    dictionary_method_as_iterator,
    "Dictionary>>asIterator",
    "(self)",
    "Return an iterator over dictionary associations."
){
    LT_Value cursor = arguments;
    LT_Dictionary* dictionary;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, dictionary, LT_Dictionary*, dictionary_from_value);
    LT_ARG_END(cursor);
    if (LT_Dictionary_size(dictionary) == 0){
        return (LT_Value)(uintptr_t)LT_EmptyIterator_instance();
    }
    return (LT_Value)(uintptr_t)dictionary_iterator_new(dictionary);
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

LT_DEFINE_PRIMITIVE(
    dictionary_iterator_method_this,
    "DictionaryIterator>>this",
    "(self)",
    "Return the current association."
){
    LT_Value cursor = arguments;
    LT_DictionaryIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(
        cursor,
        iterator,
        LT_DictionaryIterator*,
        LT_DictionaryIterator_from_value
    );
    LT_ARG_END(cursor);
    if (iterator->current == LT_INVALID){
        LT_error("DictionaryIterator is not positioned");
    }
    return iterator->current;
}

LT_DEFINE_PRIMITIVE(
    dictionary_iterator_method_has_this,
    "DictionaryIterator>>hasThis?",
    "(self)",
    "Return true when the iterator has a current association."
){
    LT_Value cursor = arguments;
    LT_DictionaryIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(
        cursor,
        iterator,
        LT_DictionaryIterator*,
        LT_DictionaryIterator_from_value
    );
    LT_ARG_END(cursor);
    return iterator->current == LT_INVALID ? LT_FALSE : LT_TRUE;
}

LT_DEFINE_PRIMITIVE(
    dictionary_iterator_method_next,
    "DictionaryIterator>>next!",
    "(self)",
    "Advance the iterator and return receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_DictionaryIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    iterator = LT_DictionaryIterator_from_value(self);
    dictionary_iterator_advance(iterator);
    return self;
}

static LT_Method_Descriptor Dictionary_methods[] = {
    {"at:put:", &dictionary_method_at_put},
    {"remove:", &dictionary_method_remove},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor ImmutableDictionary_methods[] = {
    {"size", &dictionary_method_size},
    {"at:", &dictionary_method_at},
    {"asAList", &dictionary_method_as_alist},
    {"contains?:", &dictionary_method_contains},
    {"forEach:", &dictionary_method_for_each},
    {"map:", &dictionary_method_map},
    {"asIterator", &dictionary_method_as_iterator},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor DictionaryIterator_methods[] = {
    {"this", &dictionary_iterator_method_this},
    {"hasThis?", &dictionary_iterator_method_has_this},
    {"next!", &dictionary_iterator_method_next},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor ImmutableDictionary_class_methods[] = {
    {"new", &immutable_dictionary_class_method_new},
    {"newFromAList:", &immutable_dictionary_class_method_new_from_alist},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

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

static LT_Method_Descriptor Dictionary_class_methods[] = {
    {"new", &dictionary_class_method_new},
    {"newFromAList:", &dictionary_class_method_new_from_alist},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_ImmutableDictionary) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "ImmutableDictionary",
    .documentation = "Immutable mapping from keys to values.",
    .instance_size = sizeof(LT_Dictionary),
    .class_flags = LT_CLASS_FLAG_IMMUTABLE,
    .debugPrintOn = ImmutableDictionary_debugPrintOn,
    .methods = ImmutableDictionary_methods,
    .class_methods = ImmutableDictionary_class_methods,
};

LT_DEFINE_CLASS(LT_Dictionary) {
    .superclass = &LT_ImmutableDictionary_class,
    .metaclass_superclass = &LT_ImmutableDictionary_class_class,
    .name = "Dictionary",
    .documentation = "Mutable mapping from keys to values.",
    .instance_size = sizeof(LT_Dictionary),
    .debugPrintOn = Dictionary_debugPrintOn,
    .methods = Dictionary_methods,
    .class_methods = Dictionary_class_methods,
};

LT_DEFINE_CLASS(LT_DictionaryIterator) {
    .superclass = &LT_Iterator_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "DictionaryIterator",
    .documentation = "Iterator over dictionary associations.",
    .instance_size = sizeof(LT_DictionaryIterator),
    .debugPrintOn = DictionaryIterator_debugPrintOn,
    .methods = DictionaryIterator_methods,
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
    LT_InlineHash_Entry* entry;

    LT_Mutex_lock(&table->lock);
    entry = dictionary_find_entry(dictionary, key, hash, &previous, &index);
    if (entry == NULL){
        LT_Mutex_unlock(&table->lock);
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
    LT_Mutex_unlock(&table->lock);
    return 1;
}
