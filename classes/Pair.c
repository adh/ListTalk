/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Pair.h>
#include <ListTalk/vm/Class.h>

struct LT_Pair_s {
    LT_Value car;
    LT_Value cdr;
};

static void Pair_print_value(LT_Value value, FILE* stream);

static void Pair_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Pair* pair = LT_Pair_from_object(obj);
    fputc('(', stream);
    while (1){
        Pair_print_value(pair->car, stream);

        if (pair->cdr == LT_VALUE_NIL){
            break;
        }
        if (LT_Value_is_pair(pair->cdr)){
            fputc(' ', stream);
            pair = LT_Pair_from_object(pair->cdr);
            continue;
        }

        fputs(" . ", stream);
        Pair_print_value(pair->cdr, stream);
        break;
    }
    fputc(')', stream);
}

static void Pair_print_value(LT_Value value, FILE* stream){
    LT_Class* klass;

    klass = LT_Value_class(value);
    if (klass != NULL && klass->debugPrintOn != NULL){
        klass->debugPrintOn(value, stream);
        return;
    }

    fputs("<value>", stream);
}

LT_DEFINE_CLASS(LT_Pair) {
    .superclass = &LT_Class_class,
    .metaclass_superclass = &LT_Class_class,
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
