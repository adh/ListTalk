/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Float.h>
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

LT_Value LT_Number_add2(LT_Value left, LT_Value right){
    int64_t result;

    if (LT_Value_is_fixnum(left) && LT_Value_is_fixnum(right)){
        int64_t left_value = LT_SmallInteger_value(left);
        int64_t right_value = LT_SmallInteger_value(right);

        if (__builtin_add_overflow(left_value, right_value, &result)){
            return LT_Float_new((double)left_value + (double)right_value);
        }
        return checked_fixnum_from_i64(result);
    }

    if (LT_Float_p(left) && LT_Float_p(right)){
        return LT_Float_new(LT_Float_value(left) + LT_Float_value(right));
    }

    if (LT_Value_is_fixnum(left) && LT_Float_p(right)){
        return LT_Float_new((double)LT_SmallInteger_value(left) + LT_Float_value(right));
    }
    if (LT_Float_p(left) && LT_Value_is_fixnum(right)){
        return LT_Float_new(LT_Float_value(left) + (double)LT_SmallInteger_value(right));
    }

    LT_type_error(
        !LT_Value_is_fixnum(left) && !LT_Float_p(left) ? left : right,
        &LT_Number_class
    );
    return LT_NIL;
}

LT_Value LT_Number_subtract2(LT_Value left, LT_Value right){
    int64_t result;

    if (LT_Value_is_fixnum(left) && LT_Value_is_fixnum(right)){
        int64_t left_value = LT_SmallInteger_value(left);
        int64_t right_value = LT_SmallInteger_value(right);

        if (__builtin_sub_overflow(left_value, right_value, &result)){
            return LT_Float_new((double)left_value - (double)right_value);
        }
        return checked_fixnum_from_i64(result);
    }

    if (LT_Float_p(left) && LT_Float_p(right)){
        return LT_Float_new(LT_Float_value(left) - LT_Float_value(right));
    }

    if (LT_Value_is_fixnum(left) && LT_Float_p(right)){
        return LT_Float_new((double)LT_SmallInteger_value(left) - LT_Float_value(right));
    }
    if (LT_Float_p(left) && LT_Value_is_fixnum(right)){
        return LT_Float_new(LT_Float_value(left) - (double)LT_SmallInteger_value(right));
    }

    LT_type_error(
        !LT_Value_is_fixnum(left) && !LT_Float_p(left) ? left : right,
        &LT_Number_class
    );
    return LT_NIL;
}

LT_Value LT_Number_multiply2(LT_Value left, LT_Value right){
    int64_t result;

    if (LT_Value_is_fixnum(left) && LT_Value_is_fixnum(right)){
        int64_t left_value = LT_SmallInteger_value(left);
        int64_t right_value = LT_SmallInteger_value(right);

        if (__builtin_mul_overflow(left_value, right_value, &result)){
            return LT_Float_new((double)left_value * (double)right_value);
        }
        return checked_fixnum_from_i64(result);
    }

    if (LT_Float_p(left) && LT_Float_p(right)){
        return LT_Float_new(LT_Float_value(left) * LT_Float_value(right));
    }

    if (LT_Value_is_fixnum(left) && LT_Float_p(right)){
        return LT_Float_new((double)LT_SmallInteger_value(left) * LT_Float_value(right));
    }
    if (LT_Float_p(left) && LT_Value_is_fixnum(right)){
        return LT_Float_new(LT_Float_value(left) * (double)LT_SmallInteger_value(right));
    }

    LT_type_error(
        !LT_Value_is_fixnum(left) && !LT_Float_p(left) ? left : right,
        &LT_Number_class
    );
    return LT_NIL;
}

LT_Value LT_Number_divide2(LT_Value left, LT_Value right){
    if (LT_Float_p(left) && LT_Float_p(right)){
        double divisor = LT_Float_value(right);

        if (divisor == 0.0){
            LT_error("Division by zero");
        }
        return LT_Float_new(LT_Float_value(left) / divisor);
    }

    if (LT_Value_is_fixnum(left) && LT_Float_p(right)){
        double divisor = LT_Float_value(right);

        if (divisor == 0.0){
            LT_error("Division by zero");
        }
        return LT_Float_new((double)LT_SmallInteger_value(left) / divisor);
    }
    if (LT_Float_p(left) && LT_Value_is_fixnum(right)){
        int64_t divisor = LT_SmallInteger_value(right);

        if (divisor == 0){
            LT_error("Division by zero");
        }
        return LT_Float_new(LT_Float_value(left) / (double)divisor);
    }
    if (LT_Value_is_fixnum(left) && LT_Value_is_fixnum(right)){
        int64_t left_value = LT_SmallInteger_value(left);
        int64_t right_value = LT_SmallInteger_value(right);

        if (right_value == 0){
            LT_error("Division by zero");
        }
        if (left_value == INT64_MIN && right_value == -1){
            return LT_Float_new((double)left_value / (double)right_value);
        }

        return checked_fixnum_from_i64(left_value / right_value);
    }

    LT_type_error(
        !LT_Value_is_fixnum(left) && !LT_Float_p(left) ? left : right,
        &LT_Number_class
    );
    return LT_NIL;
}

LT_Value LT_Number_negate(LT_Value value){
    int64_t result;

    if (LT_Float_p(value)){
        return LT_Float_new(-LT_Float_value(value));
    }
    if (!LT_Value_is_fixnum(value)){
        LT_type_error(value, &LT_Number_class);
        return LT_NIL;
    }

    if (__builtin_sub_overflow((int64_t)0, LT_SmallInteger_value(value), &result)){
        return LT_Float_new(-(double)LT_SmallInteger_value(value));
    }

    return checked_fixnum_from_i64(result);
}
