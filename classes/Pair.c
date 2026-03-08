/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Pair.h>
#include <ListTalk/vm/Class.h>

#include <stdarg.h>

struct LT_Pair_s {
    LT_Value car;
    LT_Value cdr;
};

static void Pair_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Pair* pair = LT_Pair_from_object(obj);
    fputc('(', stream);
    while (1){
        LT_Value_debugPrintOn(pair->car, stream);

        if (pair->cdr == LT_VALUE_NIL){
            break;
        }
        if (LT_Value_is_pair(pair->cdr)){
            fputc(' ', stream);
            pair = LT_Pair_from_object(pair->cdr);
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
    .debugPrintOn = Pair_debugPrintOn,
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
    return LT_Pair_from_object(pair)->car;
}

LT_Value LT_cdr(LT_Value pair){
    return LT_Pair_from_object(pair)->cdr;
}

void LT_Pair_set_car(LT_Value pair, LT_Value value){
    LT_Pair_from_object(pair)->car = value;
}

void LT_Pair_set_cdr(LT_Value pair, LT_Value value){
    LT_Pair_from_object(pair)->cdr = value;
}
