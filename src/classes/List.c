/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/List.h>
#include <ListTalk/classes/ImmutableList.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/error.h>

static int list_instance_p(LT_Value value){
    return LT_Value_is_instance_of(value, LT_STATIC_CLASS(LT_List));
}

static size_t list_length(LT_Value value){
    size_t length = 0;

    while (list_instance_p(value)){
        length++;
        value = LT_cdr(value);
    }

    if (value != LT_NIL){
        LT_error("List length requires proper list");
    }
    return length;
}

size_t LT_List_hash(LT_Value value){
    size_t hash = 0;

    while (list_instance_p(value)){
        size_t car_hash = LT_Value_hash(LT_car(value));

        hash = (hash * (size_t)33) ^ (car_hash + (size_t)0x9e3779b1);
        value = LT_cdr(value);
    }

    return (hash * (size_t)33) ^ (LT_Value_hash(value) + (size_t)0x9e3779b1);
}

int LT_List_equal_p(LT_Value left, LT_Value right){
    while (list_instance_p(left) && list_instance_p(right)){
        if (!LT_Value_equal_p(LT_car(left), LT_car(right))){
            return 0;
        }
        left = LT_cdr(left);
        right = LT_cdr(right);
    }

    if (list_instance_p(left) || list_instance_p(right)){
        return 0;
    }

    return LT_Value_equal_p(left, right);
}

void LT_List_debugPrintOn(LT_Value value, FILE* stream){
    fputc('(', stream);
    while (1){
        LT_Value_debugPrintOn(LT_car(value), stream);
        value = LT_cdr(value);

        if (value == LT_NIL){
            break;
        }
        if (list_instance_p(value)){
            fputc(' ', stream);
            continue;
        }

        fputs(" . ", stream);
        LT_Value_debugPrintOn(value, stream);
        break;
    }
    fputc(')', stream);
}

LT_DEFINE_PRIMITIVE(
    list_method_car,
    "List>>car",
    "(self)",
    "Return list car."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_car(self);
}

LT_DEFINE_PRIMITIVE(
    list_method_cdr,
    "List>>cdr",
    "(self)",
    "Return list cdr."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_cdr(self);
}

LT_DEFINE_PRIMITIVE(
    list_method_length,
    "List>>length",
    "(self)",
    "Return list length."
){
    LT_Value cursor = arguments;
    LT_Value self;
    size_t length;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    length = list_length(self);
    if (!LT_SmallInteger_in_range((int64_t)length)){
        LT_error("List length does not fit fixnum");
    }
    return LT_SmallInteger_new((int64_t)length);
}

static LT_Method_Descriptor List_methods[] = {
    {"car", &list_method_car},
    {"cdr", &list_method_cdr},
    {"length", &list_method_length},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_List) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "List",
    .instance_size = 0,
    .class_flags = LT_CLASS_FLAG_SPECIAL | LT_CLASS_FLAG_ABSTRACT,
    .hash = LT_List_hash,
    .equal_p = LT_List_equal_p,
    .debugPrintOn = LT_List_debugPrintOn,
    .methods = List_methods,
};
