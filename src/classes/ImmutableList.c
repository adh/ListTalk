/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/List.h>
#include <ListTalk/classes/ImmutableList.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/decl_macros.h>

#include <stddef.h>

static int immutable_list_like_p(LT_Value value){
    return LT_Value_is_instance_of(value, LT_STATIC_CLASS(LT_List));
}

static LT_Value immutable_list_terminator(unsigned int flags){
    return LT_VALUE_MAKE_IMMEDIATE(LT_VALUE_IMMEDIATE_TAG_INVALID, flags);
}

static int immutable_list_is_terminator(LT_Value value){
    return LT_VALUE_IS_IMMEDIATE(value)
        && LT_VALUE_IMMEDIATE_TAG(value) == LT_VALUE_IMMEDIATE_TAG_INVALID;
}

static LT_Value* immutable_list_values(LT_Value value){
    return (LT_Value*)LT_VALUE_POINTER_VALUE(value);
}

static size_t immutable_list_length(LT_Value value){
    LT_Value* values = immutable_list_values(value);
    size_t count = 0;

    while (!immutable_list_is_terminator(values[count])){
        count++;
    }
    return count;
}

static LT_Value immutable_list_tail(LT_Value value){
    LT_Value* values = immutable_list_values(value);
    size_t length = immutable_list_length(value);
    LT_Value terminator = values[length];
    uintptr_t flags = LT_VALUE_IMMEDIATE_VALUE(terminator);

    if ((flags & LT_IMMUTABLE_LIST_FLAG_CDR_TAIL) == 0){
        return LT_NIL;
    }

    return values[length + 1];
}

static size_t ImmutableList_hash(LT_Value value){
    return LT_List_hash(value);
}

static int ImmutableList_equal_p(LT_Value left, LT_Value right){
    return LT_List_equal_p(left, right);
}

static void ImmutableList_debugPrintOn(LT_Value obj, FILE* stream){
    LT_List_debugPrintOn(obj, stream);
}

LT_DEFINE_PRIMITIVE(
    immutable_list_class_method_from_list,
    "ImmutableList class>>fromList:",
    "(self list)",
    "Return an immutable list copy of list."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value list;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, list);
    LT_ARG_END(cursor);

    if (self != (LT_Value)(uintptr_t)&LT_ImmutableList_class){
        LT_error("fromList: class method is only supported on ImmutableList");
    }

    return LT_ImmutableList_fromList(list);
}

LT_DEFINE_PRIMITIVE(
    immutable_list_method_car,
    "ImmutableList>>car",
    "(self)",
    "Return immutable list car."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_ImmutableList_car(self);
}

LT_DEFINE_PRIMITIVE(
    immutable_list_method_cdr,
    "ImmutableList>>cdr",
    "(self)",
    "Return immutable list cdr."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_ImmutableList_cdr(self);
}

static LT_Method_Descriptor ImmutableList_methods[] = {
    {"car", &immutable_list_method_car},
    {"cdr", &immutable_list_method_cdr},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor ImmutableList_class_methods[] = {
    {"fromList:", &immutable_list_class_method_from_list},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_ImmutableList) {
    .superclass = &LT_List_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "ImmutableList",
    .instance_size = 0,
    .class_flags = LT_CLASS_FLAG_SPECIAL | LT_CLASS_FLAG_FINAL | LT_CLASS_FLAG_IMMUTABLE,
    .hash = ImmutableList_hash,
    .equal_p = ImmutableList_equal_p,
    .debugPrintOn = ImmutableList_debugPrintOn,
    .methods = ImmutableList_methods,
    .class_methods = ImmutableList_class_methods,
};

LT_Value LT_ImmutableList_new(size_t count, const LT_Value* values){
    return LT_ImmutableList_new_with_tail(count, values, LT_NIL);
}

LT_Value LT_ImmutableList_new_with_tail(
    size_t count,
    const LT_Value* values,
    LT_Value tail
){
    LT_Value* storage;
    size_t extra_values = (tail != LT_NIL) ? 1 : 0;
    size_t i;

    if (count == 0){
        return tail;
    }

    storage = GC_MALLOC(sizeof(LT_Value) * (count + 1 + extra_values));
    for (i = 0; i < count; i++){
        storage[i] = values[i];
    }
    storage[count] = immutable_list_terminator(
        (tail != LT_NIL) ? LT_IMMUTABLE_LIST_FLAG_CDR_TAIL : 0
    );
    if (tail != LT_NIL){
        storage[count + 1] = tail;
    }

    return ((LT_Value)(uintptr_t)storage) | LT_VALUE_POINTER_TAG_IMMUTABLE_LIST;
}

LT_Value LT_ImmutableList_fromList(LT_Value list){
    LT_Value cursor = list;
    LT_Value tail;
    LT_Value* values;
    size_t count = 0;
    size_t i;

    if (list == LT_NIL || LT_ImmutableList_p(list)){
        return list;
    }
    if (!immutable_list_like_p(list)){
        LT_type_error(list, &LT_List_class);
    }

    while (immutable_list_like_p(cursor)){
        count++;
        cursor = LT_cdr(cursor);
    }
    tail = cursor;

    values = GC_MALLOC(sizeof(LT_Value) * count);
    cursor = list;
    for (i = 0; i < count; i++){
        values[i] = LT_car(cursor);
        cursor = LT_cdr(cursor);
    }

    return LT_ImmutableList_new_with_tail(count, values, tail);
}

LT_Value LT_ImmutableList_car(LT_Value value){
    LT_Value* values;

    values = immutable_list_values(value);
    if (immutable_list_is_terminator(values[0])){
        LT_error("car expects non-empty pair or immutable list");
    }
    return values[0];
}

LT_Value LT_ImmutableList_cdr(LT_Value value){
    LT_Value* values;

    values = immutable_list_values(value);
    if (immutable_list_is_terminator(values[0])){
        LT_error("cdr expects non-empty pair or immutable list");
    }
    if (!immutable_list_is_terminator(values[1])){
        return (((LT_Value)(uintptr_t)(values + 1))
            | LT_VALUE_POINTER_TAG_IMMUTABLE_LIST);
    }
    return immutable_list_tail(value);
}
