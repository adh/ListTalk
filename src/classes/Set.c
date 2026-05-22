/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/IdentitySet.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Set.h>
#include <ListTalk/classes/WeakIdentitySet.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/weak.h>

#include <stdint.h>
#include <string.h>

struct LT_Set_s {
    LT_Object base;
    LT_InlineHash table;
};

struct LT_IdentitySet_s {
    LT_Object base;
    LT_InlineHash table;
};

struct LT_WeakIdentitySet_s {
    LT_Object base;
    LT_InlineHash table;
};

static LT_Set* set_from_value(LT_Value obj){
    if (!LT_Value_is_instance_of(obj, (LT_Value)(uintptr_t)&LT_Set_class)){
        LT_type_error(obj, &LT_Set_class);
    }
    return (LT_Set*)LT_VALUE_POINTER_VALUE(obj);
}

static int set_identity_p(LT_Set* set){
    return set->base.klass == &LT_IdentitySet_class
        || set->base.klass == &LT_WeakIdentitySet_class;
}

static int set_weak_identity_p(LT_Set* set){
    return set->base.klass == &LT_WeakIdentitySet_class;
}

static int set_entry_value(LT_Set* set,
                           LT_InlineHash_Entry* entry,
                           LT_Value* value_out){
    if (set_weak_identity_p(set)){
        LT_WeakValue* weak = (LT_WeakValue*)entry->key;

        if (!LT_weak_is_alive(*weak)){
            return 0;
        }
        *value_out = LT_weak_unbox(*weak);
        return 1;
    }

    *value_out = (LT_Value)(uintptr_t)entry->key;
    return 1;
}

static size_t set_value_hash(LT_Set* set, LT_Value value){
    if (set_identity_p(set)){
        return LT_pointer_hash((void*)(uintptr_t)value);
    }
    return LT_Value_hash(value);
}

static int set_value_equal_p(LT_Set* set, LT_Value left, LT_Value right){
    if (set_identity_p(set)){
        return left == right;
    }
    return LT_Value_equal_p(left, right);
}

static void Set_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Set* set = set_from_value(obj);
    fprintf(stream, "#<Set %p size=%zu>",
        (void*)set,
        LT_InlineHash_count(&set->table)
    );
}

static void IdentitySet_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Set* set = set_from_value(obj);
    fprintf(stream, "#<IdentitySet %p size=%zu>",
        (void*)set,
        LT_InlineHash_count(&set->table)
    );
}

static void WeakIdentitySet_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Set* set = set_from_value(obj);
    fprintf(stream, "#<WeakIdentitySet %p size=%zu>",
        (void*)set,
        LT_InlineHash_count(&set->table)
    );
}

static void set_grow_table(LT_Set* set){
    LT_InlineHash* table = &set->table;
    LT_InlineHash_Entry** grown_vector;
    size_t grown_size;
    size_t i;
    size_t grown_count = 0;

    grown_size = (table->mask + 1) << 1;
    grown_vector = GC_MALLOC(sizeof(LT_InlineHash_Entry*) * grown_size);
    memset(grown_vector, 0, sizeof(LT_InlineHash_Entry*) * grown_size);

    for (i = 0; i < table->mask + 1; i++){
        LT_InlineHash_Entry* entry = table->vector[i];

        while (entry != NULL){
            LT_InlineHash_Entry* next = entry->next;
            size_t index = entry->hash & (grown_size - 1);
            LT_Value value;

            if (set_entry_value(set, entry, &value)){
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

static LT_InlineHash_Entry* set_find_entry(LT_Set* set, LT_Value value, size_t hash){
    LT_InlineHash* table = &set->table;
    LT_InlineHash_Entry* entry = table->vector[hash & table->mask];

    while (entry != NULL){
        LT_Value entry_key;

        if (entry->hash == hash
            && set_entry_value(set, entry, &entry_key)
            && set_value_equal_p(set, entry_key, value)){
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

static LT_Value set_apply1(LT_Value callable, LT_Value value){
    return LT_apply(callable, LT_cons(value, LT_NIL), LT_NIL, LT_NIL, NULL);
}

static LT_Value set_apply2(LT_Value callable, LT_Value left, LT_Value right){
    return LT_apply(
        callable,
        LT_cons(left, LT_cons(right, LT_NIL)),
        LT_NIL,
        LT_NIL,
        NULL
    );
}

LT_Set* LT_Set_new(void){
    LT_Set* set = LT_Class_ALLOC(LT_Set);
    LT_InlineHash_init(&set->table);
    return set;
}

LT_IdentitySet* LT_IdentitySet_new(void){
    LT_IdentitySet* set = LT_Class_ALLOC(LT_IdentitySet);
    LT_InlineHash_init(&set->table);
    return set;
}

LT_WeakIdentitySet* LT_WeakIdentitySet_new(void){
    LT_WeakIdentitySet* set = LT_Class_ALLOC(LT_WeakIdentitySet);
    LT_InlineHash_init(&set->table);
    return set;
}

LT_Set* LT_Set_fromList(LT_Value list){
    LT_Set* set = LT_Set_new();

    while (LT_Pair_p(list)){
        LT_Set_put(set, LT_car(list));
        list = LT_cdr(list);
    }
    if (list != LT_NIL){
        LT_error("Set fromList: expects proper list");
    }

    return set;
}

LT_IdentitySet* LT_IdentitySet_fromList(LT_Value list){
    LT_IdentitySet* identity_set = LT_IdentitySet_new();
    LT_Set* set = (LT_Set*)identity_set;

    while (LT_Pair_p(list)){
        LT_Set_put(set, LT_car(list));
        list = LT_cdr(list);
    }
    if (list != LT_NIL){
        LT_error("IdentitySet fromList: expects proper list");
    }

    return identity_set;
}

LT_WeakIdentitySet* LT_WeakIdentitySet_fromList(LT_Value list){
    LT_WeakIdentitySet* identity_set = LT_WeakIdentitySet_new();
    LT_Set* set = (LT_Set*)identity_set;

    while (LT_Pair_p(list)){
        LT_Set_put(set, LT_car(list));
        list = LT_cdr(list);
    }
    if (list != LT_NIL){
        LT_error("WeakIdentitySet fromList: expects proper list");
    }

    return identity_set;
}

size_t LT_Set_size(LT_Set* set){
    return LT_InlineHash_count(&set->table);
}

int LT_Set_put(LT_Set* set, LT_Value value){
    LT_InlineHash* table = &set->table;
    size_t hash = set_value_hash(set, value);
    LT_InlineHash_Entry* entry = set_find_entry(set, value, hash);

    if (entry != NULL){
        return 0;
    }

    if (table->count > table->mask){
        set_grow_table(set);
    }

    entry = GC_NEW(LT_InlineHash_Entry);
    entry->hash = hash;
    if (set_weak_identity_p(set)){
        LT_WeakValue* weak = GC_NEW(LT_WeakValue);

        LT_weak_box(weak, value);
        entry->key = weak;
    } else {
        entry->key = (void*)(uintptr_t)value;
    }
    entry->value = NULL;
    entry->next = table->vector[hash & table->mask];
    table->vector[hash & table->mask] = entry;
    table->count++;
    return 1;
}

int LT_Set_contains(LT_Set* set, LT_Value value){
    return set_find_entry(set, value, set_value_hash(set, value)) != NULL;
}

LT_Value LT_Set_asList(LT_Set* set){
    LT_InlineHash* table = &set->table;
    LT_Value list = LT_NIL;
    size_t i;

    for (i = 0; i < table->mask + 1; i++){
        LT_InlineHash_Entry* entry = table->vector[i];

        while (entry != NULL){
            LT_Value value;

            if (set_entry_value(set, entry, &value)){
                list = LT_cons(value, list);
            }
            entry = entry->next;
        }
    }
    return list;
}

void LT_Set_for_each(LT_Set* set, LT_Value callable){
    LT_InlineHash* table = &set->table;
    size_t i;

    for (i = 0; i < table->mask + 1; i++){
        LT_InlineHash_Entry* entry = table->vector[i];

        while (entry != NULL){
            LT_Value value;

            if (set_entry_value(set, entry, &value)){
                (void)set_apply1(callable, value);
            }
            entry = entry->next;
        }
    }
}

LT_Value LT_Set_any(LT_Set* set, LT_Value callable){
    LT_InlineHash* table = &set->table;
    size_t i;

    for (i = 0; i < table->mask + 1; i++){
        LT_InlineHash_Entry* entry = table->vector[i];

        while (entry != NULL){
            LT_Value value;

            if (set_entry_value(set, entry, &value)
                && LT_Value_truthy_p(set_apply1(callable, value))){
                return LT_TRUE;
            }
            entry = entry->next;
        }
    }
    return LT_FALSE;
}

LT_Value LT_Set_every(LT_Set* set, LT_Value callable){
    LT_InlineHash* table = &set->table;
    size_t i;

    for (i = 0; i < table->mask + 1; i++){
        LT_InlineHash_Entry* entry = table->vector[i];

        while (entry != NULL){
            LT_Value value;

            if (set_entry_value(set, entry, &value)
                && !LT_Value_truthy_p(set_apply1(callable, value))){
                return LT_FALSE;
            }
            entry = entry->next;
        }
    }
    return LT_TRUE;
}

LT_Value LT_Set_inject_into(LT_Set* set, LT_Value initial, LT_Value callable){
    LT_InlineHash* table = &set->table;
    LT_Value accumulator = initial;
    size_t i;

    for (i = 0; i < table->mask + 1; i++){
        LT_InlineHash_Entry* entry = table->vector[i];

        while (entry != NULL){
            LT_Value value;

            if (set_entry_value(set, entry, &value)){
                accumulator = set_apply2(callable, accumulator, value);
            }
            entry = entry->next;
        }
    }
    return accumulator;
}

LT_Value LT_Set_reduce(LT_Set* set, LT_Value callable){
    LT_InlineHash* table = &set->table;
    LT_Value accumulator = LT_NIL;
    int has_accumulator = 0;
    size_t i;

    for (i = 0; i < table->mask + 1; i++){
        LT_InlineHash_Entry* entry = table->vector[i];

        while (entry != NULL){
            LT_Value value;

            if (set_entry_value(set, entry, &value)){
                if (!has_accumulator){
                    accumulator = value;
                    has_accumulator = 1;
                } else {
                    accumulator = set_apply2(callable, accumulator, value);
                }
            }
            entry = entry->next;
        }
    }

    if (!has_accumulator){
        LT_error("Set reduce: expects at least one element");
    }
    return accumulator;
}

LT_DEFINE_PRIMITIVE(
    set_class_method_new,
    "Set class>>new",
    "(self)",
    "Return a new empty set."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    if (self != (LT_Value)(uintptr_t)&LT_Set_class){
        LT_error("new class method is only supported on Set");
    }

    return (LT_Value)(uintptr_t)LT_Set_new();
}

LT_DEFINE_PRIMITIVE(
    identity_set_class_method_new,
    "IdentitySet class>>new",
    "(self)",
    "Return a new empty identity set."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    if (self != (LT_Value)(uintptr_t)&LT_IdentitySet_class){
        LT_error("new class method is only supported on IdentitySet");
    }

    return (LT_Value)(uintptr_t)LT_IdentitySet_new();
}

LT_DEFINE_PRIMITIVE(
    weak_identity_set_class_method_new,
    "WeakIdentitySet class>>new",
    "(self)",
    "Return a new empty weak identity set."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    if (self != (LT_Value)(uintptr_t)&LT_WeakIdentitySet_class){
        LT_error("new class method is only supported on WeakIdentitySet");
    }

    return (LT_Value)(uintptr_t)LT_WeakIdentitySet_new();
}

LT_DEFINE_PRIMITIVE(
    set_class_method_from_list,
    "Set class>>fromList:",
    "(self list)",
    "Return a set containing the unique elements of list."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value list;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, list);
    LT_ARG_END(cursor);

    if (self != (LT_Value)(uintptr_t)&LT_Set_class){
        LT_error("fromList: class method is only supported on Set");
    }

    return (LT_Value)(uintptr_t)LT_Set_fromList(list);
}

LT_DEFINE_PRIMITIVE(
    identity_set_class_method_from_list,
    "IdentitySet class>>fromList:",
    "(self list)",
    "Return an identity set containing the unique identities of list."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value list;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, list);
    LT_ARG_END(cursor);

    if (self != (LT_Value)(uintptr_t)&LT_IdentitySet_class){
        LT_error("fromList: class method is only supported on IdentitySet");
    }

    return (LT_Value)(uintptr_t)LT_IdentitySet_fromList(list);
}

LT_DEFINE_PRIMITIVE(
    weak_identity_set_class_method_from_list,
    "WeakIdentitySet class>>fromList:",
    "(self list)",
    "Return a weak identity set containing the unique identities of list."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value list;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, list);
    LT_ARG_END(cursor);

    if (self != (LT_Value)(uintptr_t)&LT_WeakIdentitySet_class){
        LT_error("fromList: class method is only supported on WeakIdentitySet");
    }

    return (LT_Value)(uintptr_t)LT_WeakIdentitySet_fromList(list);
}

LT_DEFINE_PRIMITIVE(
    set_method_put,
    "Set>>put:",
    "(self value)",
    "Add value to set and return it."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    LT_Set_put(set_from_value(self), value);
    return value;
}

LT_DEFINE_PRIMITIVE(
    set_method_contains,
    "Set>>contains?:",
    "(self value)",
    "Return true when set contains value."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Set_contains(set_from_value(self), value) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    set_method_as_list,
    "Set>>asList",
    "(self)",
    "Return set elements as a list."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Set_asList(set_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    set_method_for_each,
    "Set>>forEach:",
    "(self callable)",
    "Apply callable to each element and return nil."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    LT_Set_for_each(set_from_value(self), callable);
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE(
    set_method_any,
    "Set>>any:",
    "(self callable)",
    "Return true when callable returns truthy for any element."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    return LT_Set_any(set_from_value(self), callable);
}

LT_DEFINE_PRIMITIVE(
    set_method_every,
    "Set>>every:",
    "(self callable)",
    "Return true when callable returns truthy for every element."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    return LT_Set_every(set_from_value(self), callable);
}

LT_DEFINE_PRIMITIVE(
    set_method_inject_into,
    "Set>>inject:into:",
    "(self initial callable)",
    "Fold set with initial value and callable."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value initial;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, initial);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    return LT_Set_inject_into(set_from_value(self), initial, callable);
}

LT_DEFINE_PRIMITIVE(
    set_method_reduce,
    "Set>>reduce:",
    "(self callable)",
    "Reduce set with callable."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    return LT_Set_reduce(set_from_value(self), callable);
}

static LT_Method_Descriptor Set_methods[] = {
    {"put:", &set_method_put},
    {"contains?:", &set_method_contains},
    {"asList", &set_method_as_list},
    {"forEach:", &set_method_for_each},
    {"any:", &set_method_any},
    {"every:", &set_method_every},
    {"inject:into:", &set_method_inject_into},
    {"reduce:", &set_method_reduce},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor Set_class_methods[] = {
    {"new", &set_class_method_new},
    {"fromList:", &set_class_method_from_list},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor IdentitySet_class_methods[] = {
    {"new", &identity_set_class_method_new},
    {"fromList:", &identity_set_class_method_from_list},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor WeakIdentitySet_class_methods[] = {
    {"new", &weak_identity_set_class_method_new},
    {"fromList:", &weak_identity_set_class_method_from_list},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Set) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Set",
    .documentation = "Collection of unique values compared by equality.",
    .instance_size = sizeof(LT_Set),
    .debugPrintOn = Set_debugPrintOn,
    .methods = Set_methods,
    .class_methods = Set_class_methods,
};

LT_DEFINE_CLASS(LT_IdentitySet) {
    .superclass = &LT_Set_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "IdentitySet",
    .documentation = "Set of unique values compared by identity.",
    .instance_size = sizeof(LT_IdentitySet),
    .debugPrintOn = IdentitySet_debugPrintOn,
    .class_methods = IdentitySet_class_methods,
};

LT_DEFINE_CLASS(LT_WeakIdentitySet) {
    .superclass = &LT_IdentitySet_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "WeakIdentitySet",
    .documentation = "Identity set that does not keep members alive.",
    .instance_size = sizeof(LT_WeakIdentitySet),
    .debugPrintOn = WeakIdentitySet_debugPrintOn,
    .class_methods = WeakIdentitySet_class_methods,
};
