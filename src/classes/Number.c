/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "BigInteger_internal.h"

#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/Fraction.h>
#include <ListTalk/classes/SmallFraction.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/error.h>

#include <stdbool.h>
#include <math.h>
#include <string.h>

struct LT_Number_s {
    LT_Object base;
};

struct LT_Fraction_s {
    LT_Object base;
    LT_Value numerator;
    LT_Value denominator;
};

typedef struct LT_ExactRational_s {
    LT_Value numerator;
    LT_Value denominator;
} LT_ExactRational;

static int Number_class_equal_p(LT_Value left, LT_Value right){
    return LT_Number_equal_p(left, right);
}

static bool value_is_exact_integer(LT_Value value){
    return LT_Integer_p(value);
}

static bool value_is_exact_number(LT_Value value){
    return value_is_exact_integer(value)
        || LT_SmallFraction_p(value)
        || LT_Fraction_p(value);
}

static LT_Value exact_integer_divide(LT_Value dividend, LT_Value divisor){
    LT_Value quotient;
    LT_Value remainder;

    LT_Integer_divmod(dividend, divisor, &quotient, &remainder);
    if (!LT_Integer_is_zero(remainder)){
        LT_error("Expected exact integer division");
    }
    return quotient;
}

static LT_Value make_fraction(LT_Value numerator, LT_Value denominator){
    LT_Value divisor;
    LT_Fraction* fraction;
    int64_t small_numerator;
    uint32_t small_denominator;

    if (LT_Integer_is_zero(denominator)){
        LT_error("Division by zero");
    }
    if (LT_Integer_is_zero(numerator)){
        return LT_SmallInteger_new(0);
    }
    if (LT_Integer_negative_p(denominator)){
        numerator = LT_Integer_negate(numerator);
        denominator = LT_Integer_negate(denominator);
    }

    divisor = LT_Integer_gcd(LT_Integer_abs(numerator), denominator);
    numerator = exact_integer_divide(numerator, divisor);
    denominator = exact_integer_divide(denominator, divisor);

    /* SmallIntegers are immediate values (fixed bit pattern), so == is exact. */
    if (denominator == LT_SmallInteger_new(1)){
        return numerator;
    }
    if (LT_Integer_to_int64(numerator, &small_numerator)
        && LT_Integer_to_uint32(denominator, &small_denominator)
        && LT_SmallFraction_in_range(small_numerator, small_denominator)){
        return LT_SmallFraction_new(small_numerator, small_denominator);
    }

    fraction = LT_Class_ALLOC(LT_Fraction);
    fraction->numerator = numerator;
    fraction->denominator = denominator;
    return (LT_Value)(uintptr_t)fraction;
}

static LT_ExactRational exact_rational_from_value(LT_Value value){
    LT_ExactRational result;

    if (LT_Integer_p(value)){
        result.numerator = value;
        result.denominator = LT_SmallInteger_new(1);
        return result;
    }
    if (LT_SmallFraction_p(value)){
        result.numerator = LT_SmallInteger_new(LT_SmallFraction_numerator(value));
        result.denominator = LT_SmallInteger_new((int64_t)LT_SmallFraction_denominator(value));
        return result;
    }
    if (LT_Fraction_p(value)){
        result.numerator = LT_Fraction_numerator(value);
        result.denominator = LT_Fraction_denominator(value);
        return result;
    }

    LT_type_error(value, &LT_Number_class);
    result.numerator = LT_SmallInteger_new(0);
    result.denominator = LT_SmallInteger_new(1);
    return result;
}

static double exact_to_double(LT_Value value){
    LT_ExactRational rational = exact_rational_from_value(value);
    return LT_Integer_to_double(rational.numerator)
        / LT_Integer_to_double(rational.denominator);
}

static LT_Value checked_exact_add(LT_Value left, LT_Value right){
    LT_ExactRational lhs;
    LT_ExactRational rhs;
    LT_Value g, left_scaled, right_scaled, numerator, denominator;

    /* Integer + Integer: avoid all the fraction machinery */
    if (LT_Integer_p(left) && LT_Integer_p(right)){
        return make_fraction(LT_Integer_add(left, right), LT_SmallInteger_new(1));
    }

    lhs = exact_rational_from_value(left);
    rhs = exact_rational_from_value(right);

    /* GCD-optimised rational addition:
       g = gcd(b, d); result = (a*(d/g) + c*(b/g)) / (b*(d/g)) */
    g = LT_Integer_gcd(lhs.denominator, rhs.denominator);
    left_scaled  = LT_Integer_multiply(lhs.numerator,
                       exact_integer_divide(rhs.denominator, g));
    right_scaled = LT_Integer_multiply(rhs.numerator,
                       exact_integer_divide(lhs.denominator, g));
    numerator   = LT_Integer_add(left_scaled, right_scaled);
    denominator = LT_Integer_multiply(lhs.denominator,
                      exact_integer_divide(rhs.denominator, g));

    return make_fraction(numerator, denominator);
}

static LT_Value checked_exact_subtract(LT_Value left, LT_Value right){
    LT_ExactRational lhs;
    LT_ExactRational rhs;
    LT_Value g, left_scaled, right_scaled, numerator, denominator;

    if (LT_Integer_p(left) && LT_Integer_p(right)){
        return make_fraction(LT_Integer_subtract(left, right), LT_SmallInteger_new(1));
    }

    lhs = exact_rational_from_value(left);
    rhs = exact_rational_from_value(right);

    g = LT_Integer_gcd(lhs.denominator, rhs.denominator);
    left_scaled  = LT_Integer_multiply(lhs.numerator,
                       exact_integer_divide(rhs.denominator, g));
    right_scaled = LT_Integer_multiply(rhs.numerator,
                       exact_integer_divide(lhs.denominator, g));
    numerator   = LT_Integer_subtract(left_scaled, right_scaled);
    denominator = LT_Integer_multiply(lhs.denominator,
                      exact_integer_divide(rhs.denominator, g));

    return make_fraction(numerator, denominator);
}

static LT_Value checked_exact_multiply(LT_Value left, LT_Value right){
    LT_ExactRational lhs;
    LT_ExactRational rhs;

    if (LT_Integer_p(left) && LT_Integer_p(right)){
        return make_fraction(LT_Integer_multiply(left, right), LT_SmallInteger_new(1));
    }

    lhs = exact_rational_from_value(left);
    rhs = exact_rational_from_value(right);
    return make_fraction(
        LT_Integer_multiply(lhs.numerator, rhs.numerator),
        LT_Integer_multiply(lhs.denominator, rhs.denominator)
    );
}

static LT_Value checked_exact_divide(LT_Value left, LT_Value right){
    LT_ExactRational lhs;
    LT_ExactRational rhs;

    if (LT_Integer_p(left) && LT_Integer_p(right)){
        if (LT_Integer_is_zero(right)){
            LT_error("Division by zero");
        }
        return make_fraction(left, right);
    }

    lhs = exact_rational_from_value(left);
    rhs = exact_rational_from_value(right);

    if (LT_Integer_is_zero(rhs.numerator)){
        LT_error("Division by zero");
    }

    return make_fraction(
        LT_Integer_multiply(lhs.numerator, rhs.denominator),
        LT_Integer_multiply(lhs.denominator, rhs.numerator)
    );
}

static LT_Value checked_exact_negate(LT_Value value){
    if (LT_Integer_p(value)){
        return LT_Integer_negate(value);
    }
    LT_ExactRational rational = exact_rational_from_value(value);
    return make_fraction(LT_Integer_negate(rational.numerator), rational.denominator);
}

static size_t exact_number_hash(LT_Value value){
    LT_ExactRational rational = exact_rational_from_value(value);

    /* SmallIntegers are immediate values (fixed bit pattern), so == is exact. */
    if (rational.denominator == LT_SmallInteger_new(1)){
        return LT_Integer_hash(rational.numerator);
    }

    return LT_Integer_hash(rational.numerator)
        ^ (LT_Integer_hash(rational.denominator) << 1);
}

static size_t Number_class_hash(LT_Value value){
    if (value_is_exact_number(value)){
        return exact_number_hash(value);
    }

    if (LT_Float_p(value)){
        double float_value = LT_Float_value(value);
        uint64_t bits;

        if (float_value == 0.0){
            return hash_uint64(UINT64_C(0));
        }
        if (float_value >= (double)LT_VALUE_FIXNUM_MIN
            && float_value <= (double)LT_VALUE_FIXNUM_MAX){
            double integral = trunc(float_value);
            if (integral == float_value){
                return hash_uint64((uint64_t)(int64_t)integral);
            }
        }

        memcpy(&bits, &float_value, sizeof(bits));
        return hash_uint64(bits);
    }

    return 0;
}

LT_DEFINE_CLASS(LT_Number) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Number",
    .instance_size = sizeof(LT_Number),
    .hash = Number_class_hash,
    .equal_p = Number_class_equal_p,
    .class_flags = LT_CLASS_FLAG_ABSTRACT
        | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
};

int LT_Number_parse_integer_token(const char* token, LT_Value* value){
    const char* cursor = token;

    if (*cursor == '+' || *cursor == '-'){
        cursor++;
    }
    if (*cursor == '\0'){
        return 0;
    }

    while (*cursor != '\0'){
        if (*cursor < '0' || *cursor > '9'){
            return 0;
        }
        cursor++;
    }

    *value = LT_BigInteger_new_from_digits(token);
    return 1;
}

int LT_Number_parse_fraction_token(const char* token, LT_Value* value){
    const char* slash = strchr(token, '/');
    char* numerator_token;
    char* denominator_token;
    size_t numerator_length;
    LT_Value numerator;
    LT_Value denominator;

    if (slash == NULL || strchr(slash + 1, '/') != NULL){
        return 0;
    }

    numerator_length = (size_t)(slash - token);
    if (numerator_length == 0 || slash[1] == '\0'){
        return 0;
    }

    numerator_token = GC_MALLOC_ATOMIC(numerator_length + 1);
    memcpy(numerator_token, token, numerator_length);
    numerator_token[numerator_length] = '\0';
    denominator_token = GC_MALLOC_ATOMIC(strlen(slash + 1) + 1);
    strcpy(denominator_token, slash + 1);

    if (!LT_Number_parse_integer_token(numerator_token, &numerator)
        || !LT_Number_parse_integer_token(denominator_token, &denominator)){
        return 0;
    }

    *value = make_fraction(numerator, denominator);
    return 1;
}

LT_Value LT_Fraction_numerator(LT_Value value){
    return LT_Fraction_from_value(value)->numerator;
}

LT_Value LT_Fraction_denominator(LT_Value value){
    return LT_Fraction_from_value(value)->denominator;
}

bool LT_Number_equal_p(LT_Value left, LT_Value right){
    if (value_is_exact_number(left) && value_is_exact_number(right)){
        LT_ExactRational lhs = exact_rational_from_value(left);
        LT_ExactRational rhs = exact_rational_from_value(right);

        return LT_Integer_compare(lhs.numerator, rhs.numerator) == 0
            && LT_Integer_compare(lhs.denominator, rhs.denominator) == 0;
    }

    if (LT_Float_p(left) && LT_Float_p(right)){
        return LT_Float_value(left) == LT_Float_value(right);
    }

    if (value_is_exact_number(left) && LT_Float_p(right)){
        return exact_to_double(left) == LT_Float_value(right);
    }
    if (LT_Float_p(left) && value_is_exact_number(right)){
        return LT_Float_value(left) == exact_to_double(right);
    }

    return false;
}

LT_Value LT_Number_add2(LT_Value left, LT_Value right){
    if (value_is_exact_number(left) && value_is_exact_number(right)){
        return checked_exact_add(left, right);
    }

    if (LT_Float_p(left) && LT_Float_p(right)){
        return LT_Float_new(LT_Float_value(left) + LT_Float_value(right));
    }

    if (value_is_exact_number(left) && LT_Float_p(right)){
        return LT_Float_new(exact_to_double(left) + LT_Float_value(right));
    }
    if (LT_Float_p(left) && value_is_exact_number(right)){
        return LT_Float_new(LT_Float_value(left) + exact_to_double(right));
    }

    LT_type_error(
        !value_is_exact_number(left) && !LT_Float_p(left) ? left : right,
        &LT_Number_class
    );
    return LT_NIL;
}

LT_Value LT_Number_subtract2(LT_Value left, LT_Value right){
    if (value_is_exact_number(left) && value_is_exact_number(right)){
        return checked_exact_subtract(left, right);
    }

    if (LT_Float_p(left) && LT_Float_p(right)){
        return LT_Float_new(LT_Float_value(left) - LT_Float_value(right));
    }

    if (value_is_exact_number(left) && LT_Float_p(right)){
        return LT_Float_new(exact_to_double(left) - LT_Float_value(right));
    }
    if (LT_Float_p(left) && value_is_exact_number(right)){
        return LT_Float_new(LT_Float_value(left) - exact_to_double(right));
    }

    LT_type_error(
        !value_is_exact_number(left) && !LT_Float_p(left) ? left : right,
        &LT_Number_class
    );
    return LT_NIL;
}

LT_Value LT_Number_multiply2(LT_Value left, LT_Value right){
    if (value_is_exact_number(left) && value_is_exact_number(right)){
        return checked_exact_multiply(left, right);
    }

    if (LT_Float_p(left) && LT_Float_p(right)){
        return LT_Float_new(LT_Float_value(left) * LT_Float_value(right));
    }

    if (value_is_exact_number(left) && LT_Float_p(right)){
        return LT_Float_new(exact_to_double(left) * LT_Float_value(right));
    }
    if (LT_Float_p(left) && value_is_exact_number(right)){
        return LT_Float_new(LT_Float_value(left) * exact_to_double(right));
    }

    LT_type_error(
        !value_is_exact_number(left) && !LT_Float_p(left) ? left : right,
        &LT_Number_class
    );
    return LT_NIL;
}

LT_Value LT_Number_divide2(LT_Value left, LT_Value right){
    if (value_is_exact_number(left) && value_is_exact_number(right)){
        return checked_exact_divide(left, right);
    }

    if (LT_Float_p(left) && LT_Float_p(right)){
        double divisor = LT_Float_value(right);

        if (divisor == 0.0){
            LT_error("Division by zero");
        }
        return LT_Float_new(LT_Float_value(left) / divisor);
    }

    if (value_is_exact_number(left) && LT_Float_p(right)){
        double divisor = LT_Float_value(right);

        if (divisor == 0.0){
            LT_error("Division by zero");
        }
        return LT_Float_new(exact_to_double(left) / divisor);
    }
    if (LT_Float_p(left) && value_is_exact_number(right)){
        double divisor = exact_to_double(right);

        if (divisor == 0.0){
            LT_error("Division by zero");
        }
        return LT_Float_new(LT_Float_value(left) / divisor);
    }

    LT_type_error(
        !value_is_exact_number(left) && !LT_Float_p(left) ? left : right,
        &LT_Number_class
    );
    return LT_NIL;
}

LT_Value LT_Number_negate(LT_Value value){
    if (value_is_exact_number(value)){
        return checked_exact_negate(value);
    }
    if (LT_Float_p(value)){
        return LT_Float_new(-LT_Float_value(value));
    }

    LT_type_error(value, &LT_Number_class);
    return LT_NIL;
}
