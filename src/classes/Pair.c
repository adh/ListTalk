/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/ImmutableList.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/vm/Class.h>

#include <ListTalk/macros/arg_macros.h>
#include <stddef.h>
#include <stdarg.h>

static size_t Pair_hash(LT_Value value){
    return LT_List_hash(value);
}

static int Pair_equal_p(LT_Value left, LT_Value right){
    return LT_List_equal_p(left, right);
}

static LT_Slot_Descriptor Pair_slots[] = {
    {"car", offsetof(LT_Pair, car), &LT_SlotType_Object},
    {"cdr", offsetof(LT_Pair, cdr), &LT_SlotType_Object},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static void Pair_debugPrintOn(LT_Value obj, FILE* stream){
    LT_List_debugPrintOn(obj, stream);
}

LT_DEFINE_PRIMITIVE(
    pair_method_car,
    "Pair>>car",
    "(self)",
    "Return pair car."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_car(self);
}

LT_DEFINE_PRIMITIVE(
    pair_method_cdr,
    "Pair>>cdr",
    "(self)",
    "Return pair cdr."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_cdr(self);
}

LT_DEFINE_PRIMITIVE(
    pair_method_car_put,
    "Pair>>car:",
    "(self value)",
    "Set pair car."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    LT_Pair_set_car(self, value);
    return value;
}

LT_DEFINE_PRIMITIVE(
    pair_method_cdr_put,
    "Pair>>cdr:",
    "(self value)",
    "Set pair cdr."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    LT_Pair_set_cdr(self, value);
    return value;
}

static LT_Method_Descriptor Pair_methods[] = {
    {"car", &pair_method_car},
    {"cdr", &pair_method_cdr},
    {"car:", &pair_method_car_put},
    {"cdr:", &pair_method_cdr_put},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Pair) {
    .superclass = &LT_List_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Pair",
    .instance_size = sizeof(LT_Pair),
    .class_flags = LT_CLASS_FLAG_SPECIAL,
    .hash = Pair_hash,
    .equal_p = Pair_equal_p,
    .debugPrintOn = Pair_debugPrintOn,
    .slots = Pair_slots,
    .methods = Pair_methods,
};

LT_Value LT_cons(LT_Value car, LT_Value cdr){
    LT_Pair* pair = GC_NEW(LT_Pair);
    pair->car = car;
    pair->cdr = cdr;
    return ((LT_Value)(uintptr_t)pair) | LT_VALUE_POINTER_TAG_PAIR;
}

LT_Value LT_list(LT_Value first, ...){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    va_list args;
    LT_Value value;

    if (first == LT_INVALID){
        return LT_NIL;
    }

    LT_ListBuilder_append(builder, first);
    va_start(args, first);
    while (1){
        value = va_arg(args, LT_Value);
        if (value == LT_INVALID){
            break;
        }
        LT_ListBuilder_append(builder, value);
    }
    va_end(args);
    return LT_ListBuilder_valueWithRest(builder, LT_NIL);
}

LT_Value LT_list_with_rest(LT_Value first, ...){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    va_list args;
    LT_Value value;
    LT_Value rest;

    va_start(args, first);

    if (first != LT_INVALID){
        LT_ListBuilder_append(builder, first);
        while (1){
            value = va_arg(args, LT_Value);
            if (value == LT_INVALID){
                break;
            }
            LT_ListBuilder_append(builder, value);
        }
    }

    rest = va_arg(args, LT_Value);
    va_end(args);
    return LT_ListBuilder_valueWithRest(builder, rest);
}

void LT_Pair_set_car(LT_Value pair, LT_Value value){
    LT_Pair_from_value(pair)->car = value;
}

void LT_Pair_set_cdr(LT_Value pair, LT_Value value){
    LT_Pair_from_value(pair)->cdr = value;
}
