/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Pair.h>
#include <ListTalk/vm/Class.h>

#include <stddef.h>
#include <stdarg.h>

struct LT_Pair_s {
    LT_Value car;
    LT_Value cdr;
};

static size_t Pair_hash(LT_Value value){
    LT_Pair* pair = LT_Pair_from_value(value);
    size_t car_hash = LT_Value_hash(pair->car);
    size_t cdr_hash = LT_Value_hash(pair->cdr);

    return (car_hash * (size_t)33) ^ (cdr_hash + (size_t)0x9e3779b1);
}

static int Pair_equal_p(LT_Value left, LT_Value right){
    if (!LT_Pair_p(right)){
        return 0;
    }

    return LT_Value_equal_p(LT_car(left), LT_car(right))
        && LT_Value_equal_p(LT_cdr(left), LT_cdr(right));
}

static LT_Slot_Descriptor Pair_slots[] = {
    {"car", offsetof(LT_Pair, car), &LT_SlotType_Object},
    {"cdr", offsetof(LT_Pair, cdr), &LT_SlotType_Object},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static void Pair_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Pair* pair = LT_Pair_from_value(obj);
    fputc('(', stream);
    while (1){
        LT_Value_debugPrintOn(pair->car, stream);

        if (pair->cdr == LT_NIL){
            break;
        }
        if (LT_Pair_p(pair->cdr)){
            fputc(' ', stream);
            pair = LT_Pair_from_value(pair->cdr);
            continue;
        }

        fputs(" . ", stream);
        LT_Value_debugPrintOn(pair->cdr, stream);
        break;
    }
    fputc(')', stream);
}

LT_DEFINE_CLASS(LT_Pair) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Pair",
    .instance_size = sizeof(LT_Pair),
    .class_flags = LT_CLASS_FLAG_SPECIAL,
    .hash = Pair_hash,
    .equal_p = Pair_equal_p,
    .debugPrintOn = Pair_debugPrintOn,
    .slots = Pair_slots,
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

LT_Value LT_car(LT_Value pair){
    return LT_Pair_from_value(pair)->car;
}

LT_Value LT_cdr(LT_Value pair){
    return LT_Pair_from_value(pair)->cdr;
}

void LT_Pair_set_car(LT_Value pair, LT_Value value){
    LT_Pair_from_value(pair)->car = value;
}

void LT_Pair_set_cdr(LT_Value pair, LT_Value value){
    LT_Pair_from_value(pair)->cdr = value;
}
