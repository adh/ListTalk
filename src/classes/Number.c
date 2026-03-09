/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/error.h>

struct LT_Number_s {
    LT_Object base;
};

LT_DEFINE_CLASS(LT_Number) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Number",
    .instance_size = sizeof(LT_Number),
    .class_flags = LT_CLASS_FLAG_ABSTRACT
        | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
};

static LT_Value checked_fixnum_from_i64(int64_t result){
    if (!LT_SmallInteger_in_range(result)){
        LT_error("Fixnum arithmetic overflow");
    }

    return LT_SmallInteger_new(result);
}

static int64_t LT_Number_fixnum_operand_value(LT_Value value){
    return LT_SmallInteger_value(LT_SmallInteger_from_value(value));
}

LT_Value LT_Number_add2(LT_Value left, LT_Value right){
    int64_t left_value = LT_Number_fixnum_operand_value(left);
    int64_t right_value = LT_Number_fixnum_operand_value(right);
    int64_t result;

    if (__builtin_add_overflow(left_value, right_value, &result)){
        LT_error("Fixnum arithmetic overflow");
    }
    return checked_fixnum_from_i64(result);
}

LT_Value LT_Number_subtract2(LT_Value left, LT_Value right){
    int64_t left_value = LT_Number_fixnum_operand_value(left);
    int64_t right_value = LT_Number_fixnum_operand_value(right);
    int64_t result;

    if (__builtin_sub_overflow(left_value, right_value, &result)){
        LT_error("Fixnum arithmetic overflow");
    }
    return checked_fixnum_from_i64(result);
}

LT_Value LT_Number_multiply2(LT_Value left, LT_Value right){
    int64_t left_value = LT_Number_fixnum_operand_value(left);
    int64_t right_value = LT_Number_fixnum_operand_value(right);
    int64_t result;

    if (__builtin_mul_overflow(left_value, right_value, &result)){
        LT_error("Fixnum arithmetic overflow");
    }

    return checked_fixnum_from_i64(result);
}

LT_Value LT_Number_divide2(LT_Value left, LT_Value right){
    int64_t left_value = LT_Number_fixnum_operand_value(left);
    int64_t right_value = LT_Number_fixnum_operand_value(right);
    int64_t result;

    if (right_value == 0){
        LT_error("Division by zero");
    }

    result = left_value / right_value;
    return checked_fixnum_from_i64(result);
}

LT_Value LT_Number_negate(LT_Value value){
    int64_t operand = LT_Number_fixnum_operand_value(value);
    int64_t result;

    if (__builtin_sub_overflow((int64_t)0, operand, &result)){
        LT_error("Fixnum arithmetic overflow");
    }
    return checked_fixnum_from_i64(result);
}
