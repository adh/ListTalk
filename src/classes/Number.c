/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "BigInteger_internal.h"

#include <ListTalk/classes/BigInteger.h>
#include <ListTalk/classes/ComplexNumber.h>
#include <ListTalk/classes/ExactComplexNumber.h>
#include <ListTalk/classes/Integer.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/InexactComplexNumber.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/Fraction.h>
#include <ListTalk/classes/RealNumber.h>
#include <ListTalk/classes/SmallFraction.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>

#include <stdbool.h>
#include <inttypes.h>
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

bool LT_RationalNumber_value_p(LT_Value value){
    return LT_Integer_value_p(value)
        || LT_SmallFraction_p(value)
        || LT_Fraction_p(value);
}

bool LT_RealNumber_value_p(LT_Value value){
    return LT_RationalNumber_value_p(value) || LT_Float_p(value);
}

bool LT_Number_value_p(LT_Value value){
    return LT_RealNumber_value_p(value)
        || LT_ExactComplexNumber_p(value)
        || LT_InexactComplexNumber_p(value);
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

    if (LT_Integer_value_p(value)){
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

static int exact_real_nonnegative_p(LT_Value value){
    if (LT_Value_is_fixnum(value)){
        return LT_SmallInteger_value(value) >= 0;
    }
    if (LT_BigInteger_p(value)){
        return !LT_Integer_negative_p(value);
    }
    if (LT_SmallFraction_p(value)){
        return LT_SmallFraction_numerator(value) >= 0;
    }
    if (LT_Fraction_p(value)){
        LT_Value numerator = LT_Fraction_numerator(value);

        if (LT_Value_is_fixnum(numerator)){
            return LT_SmallInteger_value(numerator) >= 0;
        }
        return !LT_Integer_negative_p(numerator);
    }

    LT_type_error(value, &LT_RealNumber_class);
    return 0;
}

char* LT_Number_to_string(LT_Value value){
    if (LT_Value_is_fixnum(value)){
        return LT_sprintf("%" PRId64, LT_SmallInteger_value(value));
    }
    if (LT_BigInteger_p(value)){
        return LT_BigInteger_to_decimal_cstr(value);
    }
    if (LT_SmallFraction_p(value)){
        return LT_sprintf(
            "%" PRId64 "/%" PRIu32,
            LT_SmallFraction_numerator(value),
            LT_SmallFraction_denominator(value)
        );
    }
    if (LT_Fraction_p(value)){
        LT_StringBuilder* builder = LT_StringBuilder_new();

        LT_StringBuilder_append_str(
            builder,
            LT_Number_to_string(LT_Fraction_numerator(value))
        );
        LT_StringBuilder_append_char(builder, '/');
        LT_StringBuilder_append_str(
            builder,
            LT_Number_to_string(LT_Fraction_denominator(value))
        );
        return LT_StringBuilder_value(builder);
    }
    if (LT_Float_p(value)){
        return LT_sprintf("%.17g", LT_Float_value(value));
    }
    if (LT_ExactComplexNumber_p(value)){
        LT_StringBuilder* builder = LT_StringBuilder_new();
        LT_Value imaginary = LT_ExactComplexNumber_imaginary(value);

        LT_StringBuilder_append_str(
            builder,
            LT_Number_to_string(LT_ExactComplexNumber_real(value))
        );
        if (exact_real_nonnegative_p(imaginary)){
            LT_StringBuilder_append_char(builder, '+');
        }
        LT_StringBuilder_append_str(builder, LT_Number_to_string(imaginary));
        LT_StringBuilder_append_char(builder, 'i');
        return LT_StringBuilder_value(builder);
    }
    if (LT_InexactComplexNumber_p(value)){
        return LT_sprintf(
            "%.17g%+.17gi",
            LT_InexactComplexNumber_real(value),
            LT_InexactComplexNumber_imaginary(value)
        );
    }

    LT_type_error(value, &LT_Number_class);
    return NULL;
}

double LT_Number_to_double(LT_Value value){
    if (LT_RationalNumber_value_p(value)){
        return exact_to_double(value);
    }
    if (LT_Float_p(value)){
        return LT_Float_value(value);
    }

    LT_type_error(value, &LT_RealNumber_class);
    return 0.0;
}

static LT_Value complex_real_part(LT_Value value){
    if (LT_ExactComplexNumber_p(value)){
        return LT_ExactComplexNumber_real(value);
    }
    if (LT_InexactComplexNumber_p(value)){
        return LT_Float_new(LT_InexactComplexNumber_real(value));
    }
    if (LT_RealNumber_value_p(value)){
        return value;
    }

    LT_type_error(value, &LT_ComplexNumber_class);
    return LT_NIL;
}

static LT_Value complex_imaginary_part(LT_Value value){
    if (LT_ExactComplexNumber_p(value)){
        return LT_ExactComplexNumber_imaginary(value);
    }
    if (LT_InexactComplexNumber_p(value)){
        return LT_Float_new(LT_InexactComplexNumber_imaginary(value));
    }
    if (LT_RealNumber_value_p(value)){
        return LT_SmallInteger_new(0);
    }

    LT_type_error(value, &LT_ComplexNumber_class);
    return LT_NIL;
}

static LT_Value make_complex_from_values(LT_Value real, LT_Value imaginary){
    if (LT_RationalNumber_value_p(real) && LT_RationalNumber_value_p(imaginary)){
        return LT_ExactComplexNumber_new(real, imaginary);
    }
    if (!LT_RealNumber_value_p(real) || !LT_RealNumber_value_p(imaginary)){
        LT_type_error(!LT_RealNumber_value_p(real) ? real : imaginary, &LT_RealNumber_class);
    }
    return LT_InexactComplexNumber_new(LT_Number_to_double(real), LT_Number_to_double(imaginary));
}

static void complex_number_to_doubles(LT_Value value, double* real, double* imaginary){
    if (LT_ExactComplexNumber_p(value)){
        *real = LT_Number_to_double(LT_ExactComplexNumber_real(value));
        *imaginary = LT_Number_to_double(LT_ExactComplexNumber_imaginary(value));
        return;
    }
    if (LT_InexactComplexNumber_p(value)){
        *real = LT_InexactComplexNumber_real(value);
        *imaginary = LT_InexactComplexNumber_imaginary(value);
        return;
    }
    if (LT_RealNumber_value_p(value)){
        *real = LT_Number_to_double(value);
        *imaginary = 0.0;
        return;
    }

    LT_type_error(value, &LT_ComplexNumber_class);
}

static LT_Value checked_exact_add(LT_Value left, LT_Value right){
    LT_ExactRational lhs;
    LT_ExactRational rhs;
    LT_Value g, left_scaled, right_scaled, numerator, denominator;

    /* Integer + Integer: avoid all the fraction machinery */
    if (LT_Integer_value_p(left) && LT_Integer_value_p(right)){
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

    if (LT_Integer_value_p(left) && LT_Integer_value_p(right)){
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

    if (LT_Integer_value_p(left) && LT_Integer_value_p(right)){
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

    if (LT_Integer_value_p(left) && LT_Integer_value_p(right)){
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
    if (LT_Integer_value_p(value)){
        return LT_Integer_negate(value);
    }
    LT_ExactRational rational = exact_rational_from_value(value);
    return make_fraction(LT_Integer_negate(rational.numerator), rational.denominator);
}

static LT_Value checked_exact_abs(LT_Value value){
    if (exact_real_nonnegative_p(value)){
        return value;
    }
    return checked_exact_negate(value);
}

static LT_Value integer_add1(LT_Value value){
    return LT_Integer_add(value, LT_SmallInteger_new(1));
}

static LT_Value integer_subtract1(LT_Value value){
    return LT_Integer_subtract(value, LT_SmallInteger_new(1));
}

static int exact_rational_divmod(LT_Value value,
                                 LT_Value* quotient,
                                 LT_Value* remainder,
                                 LT_Value* denominator){
    LT_ExactRational rational = exact_rational_from_value(value);

    *denominator = rational.denominator;
    LT_Integer_divmod(rational.numerator, rational.denominator, quotient, remainder);
    return !LT_Integer_is_zero(*remainder);
}

static LT_Value double_to_integer(double value){
    if (!isfinite(value)){
        LT_error("Cannot convert non-finite float to integer");
    }
    return LT_BigInteger_new_from_digits(LT_sprintf("%.0f", value), 10);
}

static LT_Value checked_exact_floor(LT_Value value){
    LT_Value quotient;
    LT_Value remainder;
    LT_Value denominator;

    if (!exact_rational_divmod(value, &quotient, &remainder, &denominator)){
        return quotient;
    }

    (void)denominator;
    return LT_Integer_negative_p(remainder) ? integer_subtract1(quotient) : quotient;
}

static LT_Value checked_exact_truncate(LT_Value value){
    LT_Value quotient;
    LT_Value remainder;
    LT_Value denominator;

    exact_rational_divmod(value, &quotient, &remainder, &denominator);
    (void)remainder;
    (void)denominator;
    return quotient;
}

static LT_Value checked_exact_ceiling(LT_Value value){
    LT_Value quotient;
    LT_Value remainder;
    LT_Value denominator;

    if (!exact_rational_divmod(value, &quotient, &remainder, &denominator)){
        return quotient;
    }

    (void)denominator;
    return LT_Integer_negative_p(remainder) ? quotient : integer_add1(quotient);
}

static LT_Value checked_exact_round(LT_Value value){
    LT_Value quotient;
    LT_Value remainder;
    LT_Value denominator;
    LT_Value twice_remainder_abs;

    if (!exact_rational_divmod(value, &quotient, &remainder, &denominator)){
        return quotient;
    }

    twice_remainder_abs = LT_Integer_multiply(
        LT_Integer_abs(remainder),
        LT_SmallInteger_new(2)
    );
    if (LT_Integer_compare(twice_remainder_abs, denominator) < 0){
        return quotient;
    }
    return LT_Integer_negative_p(remainder)
        ? integer_subtract1(quotient)
        : integer_add1(quotient);
}

static LT_Value value_to_exact_integer(LT_Value value, int* ok){
    LT_ExactRational rational;

    *ok = 0;
    if (!LT_RationalNumber_value_p(value)){
        return LT_NIL;
    }

    rational = exact_rational_from_value(value);
    if (rational.denominator != LT_SmallInteger_new(1)){
        return LT_NIL;
    }

    *ok = 1;
    return rational.numerator;
}

static LT_Value number_integer_power(LT_Value base, LT_Value exponent_integer){
    LT_Value one = LT_SmallInteger_new(1);
    LT_Value exponent = LT_Integer_abs(exponent_integer);
    LT_Value result = one;
    LT_Value factor = base;

    while (!LT_Integer_is_zero(exponent)){
        LT_Value quotient;
        LT_Value remainder;

        LT_Integer_divmod(exponent, LT_SmallInteger_new(2), &quotient, &remainder);
        if (!LT_Integer_is_zero(remainder)){
            result = LT_Number_multiply2(result, factor);
        }
        exponent = quotient;
        if (!LT_Integer_is_zero(exponent)){
            factor = LT_Number_multiply2(factor, factor);
        }
    }

    if (LT_Integer_negative_p(exponent_integer)){
        return LT_Number_divide2(one, result);
    }

    return result;
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

static size_t hash_double(double value){
    uint64_t bits;

    if (value == 0.0){
        return hash_uint64(UINT64_C(0));
    }

    memcpy(&bits, &value, sizeof(bits));
    return hash_uint64(bits);
}

static size_t complex_number_hash(LT_Value value){
    if (LT_ExactComplexNumber_p(value)){
        return exact_number_hash(LT_ExactComplexNumber_real(value))
            ^ (exact_number_hash(LT_ExactComplexNumber_imaginary(value)) << 1);
    }

    return hash_double(LT_InexactComplexNumber_real(value))
        ^ (hash_double(LT_InexactComplexNumber_imaginary(value)) << 1);
}

static size_t Number_class_hash(LT_Value value){
    if (LT_RationalNumber_value_p(value)){
        return exact_number_hash(value);
    }
    if (LT_ExactComplexNumber_p(value) || LT_InexactComplexNumber_p(value)){
        return complex_number_hash(value);
    }

    if (LT_Float_p(value)){
        double float_value = LT_Float_value(value);
        if (float_value >= (double)LT_VALUE_FIXNUM_MIN
            && float_value <= (double)LT_VALUE_FIXNUM_MAX){
            double integral = trunc(float_value);
            if (integral == float_value){
                return hash_uint64((uint64_t)(int64_t)integral);
            }
        }
        return hash_double(float_value);
    }

    return 0;
}

static int compare_real_numbers(LT_Value left, LT_Value right){
    double l;
    double r;

    if (LT_RationalNumber_value_p(left) && LT_RationalNumber_value_p(right)){
        LT_ExactRational lhs = exact_rational_from_value(left);
        LT_ExactRational rhs = exact_rational_from_value(right);
        LT_Value lhs_scaled = LT_Integer_multiply(lhs.numerator, rhs.denominator);
        LT_Value rhs_scaled = LT_Integer_multiply(rhs.numerator, lhs.denominator);
        return LT_Integer_compare(lhs_scaled, rhs_scaled);
    }

    if (LT_Float_p(left) && LT_Float_p(right)){
        l = LT_Float_value(left);
        r = LT_Float_value(right);
        return l < r ? -1 : (l > r ? 1 : 0);
    }

    if (LT_RationalNumber_value_p(left) && LT_Float_p(right)){
        l = exact_to_double(left);
        r = LT_Float_value(right);
        return l < r ? -1 : (l > r ? 1 : 0);
    }

    if (LT_Float_p(left) && LT_RationalNumber_value_p(right)){
        l = LT_Float_value(left);
        r = exact_to_double(right);
        return l < r ? -1 : (l > r ? 1 : 0);
    }

    LT_type_error(
        !LT_RationalNumber_value_p(left) && !LT_Float_p(left) ? left : right,
        &LT_RealNumber_class
    );
    return 0;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_asString,
    "Number>>asString",
    "(self)",
    "Return receiver formatted as a string.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_String_new_cstr(LT_Number_to_string(self));
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_equal,
    "Number>>=",
    "(self other)",
    "Return true when receiver equals argument numerically.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    return LT_Number_equal_p(self, other) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_add,
    "Number>>+",
    "(self other)",
    "Return sum of receiver and argument.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    return LT_Number_add2(self, other);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_subtract,
    "Number>>-",
    "(self other)",
    "Return difference of receiver and argument.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    return LT_Number_subtract2(self, other);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_multiply,
    "Number>>*",
    "(self other)",
    "Return product of receiver and argument.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    return LT_Number_multiply2(self, other);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_divide,
    "Number>>/",
    "(self other)",
    "Return quotient of receiver divided by argument.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    return LT_Number_divide2(self, other);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_abs,
    "Number>>abs",
    "(self)",
    "Return receiver's magnitude.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Number_abs(self);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_phase,
    "Number>>phase",
    "(self)",
    "Return receiver's principal phase angle.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Number_phase(self);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_floor,
    "Number>>floor",
    "(self)",
    "Return greatest integer not greater than receiver.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Number_floor(self);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_truncate,
    "Number>>truncate",
    "(self)",
    "Return receiver truncated toward zero as an integer.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Number_truncate(self);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_ceiling,
    "Number>>ceiling",
    "(self)",
    "Return least integer not less than receiver.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Number_ceiling(self);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_round,
    "Number>>round",
    "(self)",
    "Return nearest integer to receiver, rounding halfway values away from zero.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Number_round(self);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_sqrt,
    "Number>>sqrt",
    "(self)",
    "Return principal square root of receiver.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Number_sqrt(self);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_min_colon,
    "Number>>min:",
    "(self other)",
    "Return smaller of receiver and argument.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    return LT_Number_min2(self, other);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_max_colon,
    "Number>>max:",
    "(self other)",
    "Return larger of receiver and argument.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    return LT_Number_max2(self, other);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_zero_p,
    "Number>>zero?",
    "(self)",
    "Return true when receiver is numerically zero.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Number_zero_p(self) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_positive_p,
    "Number>>positive?",
    "(self)",
    "Return true when receiver is greater than zero.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Number_positive_p(self) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_negative_p,
    "Number>>negative?",
    "(self)",
    "Return true when receiver is less than zero.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Number_negative_p(self) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_sin,
    "Number>>sin",
    "(self)",
    "Return sine of receiver.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Number_sin(self);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_cos,
    "Number>>cos",
    "(self)",
    "Return cosine of receiver.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Number_cos(self);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_tan,
    "Number>>tan",
    "(self)",
    "Return tangent of receiver.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Number_tan(self);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_log,
    "Number>>log",
    "(self)",
    "Return natural logarithm of receiver.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Number_log(self);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_expt,
    "Number>>expt",
    "(self)",
    "Return e raised to receiver.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Number_exp(self);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_expt_colon,
    "Number>>expt:",
    "(self exponent)",
    "Return receiver raised to exponent.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value exponent;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, exponent);
    LT_ARG_END(cursor);
    return LT_Number_expt(self, exponent);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_less_than,
    "Number>><",
    "(self other)",
    "Return true when receiver is less than argument.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    return compare_real_numbers(self, other) < 0 ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_greater_than,
    "Number>>>",
    "(self other)",
    "Return true when receiver is greater than argument.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    return compare_real_numbers(self, other) > 0 ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_less_than_or_equal,
    "Number>><=",
    "(self other)",
    "Return true when receiver is less than or equal to argument.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    return compare_real_numbers(self, other) <= 0 ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    number_method_greater_than_or_equal,
    "Number>>>=",
    "(self other)",
    "Return true when receiver is greater than or equal to argument.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    return compare_real_numbers(self, other) >= 0 ? LT_TRUE : LT_FALSE;
}

static LT_Method_Descriptor Number_methods[] = {
    {"asString", &number_method_asString},
    {"=",  &number_method_equal},
    {"+",  &number_method_add},
    {"-",  &number_method_subtract},
    {"*",  &number_method_multiply},
    {"/",  &number_method_divide},
    {"abs", &number_method_abs},
    {"phase", &number_method_phase},
    {"floor", &number_method_floor},
    {"truncate", &number_method_truncate},
    {"ceiling", &number_method_ceiling},
    {"round", &number_method_round},
    {"sqrt", &number_method_sqrt},
    {"min:", &number_method_min_colon},
    {"max:", &number_method_max_colon},
    {"zero?", &number_method_zero_p},
    {"positive?", &number_method_positive_p},
    {"negative?", &number_method_negative_p},
    {"sin", &number_method_sin},
    {"cos", &number_method_cos},
    {"tan", &number_method_tan},
    {"log", &number_method_log},
    {"expt", &number_method_expt},
    {"expt:", &number_method_expt_colon},
    {"<",  &number_method_less_than},
    {">",  &number_method_greater_than},
    {"<=", &number_method_less_than_or_equal},
    {">=", &number_method_greater_than_or_equal},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

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
    .methods = Number_methods,
};

static int token_char_digit_value(int ch){
    if (ch >= '0' && ch <= '9'){
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'z'){
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'Z'){
        return ch - 'A' + 10;
    }
    return -1;
}

static int token_is_integer_for_radix(const char* token, unsigned int radix){
    const char* cursor = token;

    if (*cursor == '+' || *cursor == '-'){
        cursor++;
    }
    if (*cursor == '\0'){
        return 0;
    }

    while (*cursor != '\0'){
        int value = token_char_digit_value((unsigned char)*cursor);

        if (value < 0 || (unsigned int)value >= radix){
            return 0;
        }
        cursor++;
    }

    return 1;
}

int LT_Number_parse_token_with_radix(const char* token, unsigned int radix, LT_Value* value){
    const char* slash = strchr(token, '/');
    char* numerator_token;
    char* denominator_token;
    size_t numerator_length;
    LT_Value numerator;
    LT_Value denominator;

    if (radix < 2 || radix > 36){
        LT_error("Invalid number radix");
    }

    if (slash == NULL){
        if (!token_is_integer_for_radix(token, radix)){
            return 0;
        }
        *value = LT_BigInteger_new_from_digits(token, radix);
        return 1;
    }

    if (strchr(slash + 1, '/') != NULL){
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

    if (!token_is_integer_for_radix(numerator_token, radix)
        || !token_is_integer_for_radix(denominator_token, radix)){
        return 0;
    }

    numerator = LT_BigInteger_new_from_digits(numerator_token, radix);
    denominator = LT_BigInteger_new_from_digits(denominator_token, radix);
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
    if (LT_ExactComplexNumber_p(left) && LT_ExactComplexNumber_p(right)){
        return LT_Number_equal_p(
                   LT_ExactComplexNumber_real(left),
                   LT_ExactComplexNumber_real(right)
               )
            && LT_Number_equal_p(
                   LT_ExactComplexNumber_imaginary(left),
                   LT_ExactComplexNumber_imaginary(right)
               );
    }

    if (LT_ExactComplexNumber_p(left) && LT_InexactComplexNumber_p(right)){
        return LT_Number_to_double(LT_ExactComplexNumber_real(left))
                == LT_InexactComplexNumber_real(right)
            && LT_Number_to_double(LT_ExactComplexNumber_imaginary(left))
                == LT_InexactComplexNumber_imaginary(right);
    }
    if (LT_InexactComplexNumber_p(left) && LT_ExactComplexNumber_p(right)){
        return LT_InexactComplexNumber_real(left)
                == LT_Number_to_double(LT_ExactComplexNumber_real(right))
            && LT_InexactComplexNumber_imaginary(left)
                == LT_Number_to_double(LT_ExactComplexNumber_imaginary(right));
    }

    if (LT_InexactComplexNumber_p(left) && LT_InexactComplexNumber_p(right)){
        return LT_InexactComplexNumber_real(left) == LT_InexactComplexNumber_real(right)
            && LT_InexactComplexNumber_imaginary(left) == LT_InexactComplexNumber_imaginary(right);
    }

    if (LT_RationalNumber_value_p(left) && LT_RationalNumber_value_p(right)){
        LT_ExactRational lhs = exact_rational_from_value(left);
        LT_ExactRational rhs = exact_rational_from_value(right);

        return LT_Integer_compare(lhs.numerator, rhs.numerator) == 0
            && LT_Integer_compare(lhs.denominator, rhs.denominator) == 0;
    }

    if (LT_Float_p(left) && LT_Float_p(right)){
        return LT_Float_value(left) == LT_Float_value(right);
    }

    if (LT_RationalNumber_value_p(left) && LT_Float_p(right)){
        return exact_to_double(left) == LT_Float_value(right);
    }
    if (LT_Float_p(left) && LT_RationalNumber_value_p(right)){
        return LT_Float_value(left) == exact_to_double(right);
    }

    if ((LT_Number_value_p(left) && !LT_RationalNumber_value_p(left) && !LT_Float_p(left))
        || (LT_Number_value_p(right) && !LT_RationalNumber_value_p(right) && !LT_Float_p(right))){
        return false;
    }

    return false;
}

int LT_Number_compare(LT_Value left, LT_Value right){
    return compare_real_numbers(left, right);
}

LT_Value LT_Number_add2(LT_Value left, LT_Value right){
    if ((LT_ExactComplexNumber_p(left) || LT_InexactComplexNumber_p(left)
            || LT_ExactComplexNumber_p(right) || LT_InexactComplexNumber_p(right))
        && LT_Number_value_p(left) && LT_Number_value_p(right)){
        return make_complex_from_values(
            LT_Number_add2(complex_real_part(left), complex_real_part(right)),
            LT_Number_add2(complex_imaginary_part(left), complex_imaginary_part(right))
        );
    }

    if (LT_RationalNumber_value_p(left) && LT_RationalNumber_value_p(right)){
        return checked_exact_add(left, right);
    }

    if (LT_Float_p(left) && LT_Float_p(right)){
        return LT_Float_new(LT_Float_value(left) + LT_Float_value(right));
    }

    if (LT_RationalNumber_value_p(left) && LT_Float_p(right)){
        return LT_Float_new(exact_to_double(left) + LT_Float_value(right));
    }
    if (LT_Float_p(left) && LT_RationalNumber_value_p(right)){
        return LT_Float_new(LT_Float_value(left) + exact_to_double(right));
    }

    LT_type_error(
        !LT_RationalNumber_value_p(left) && !LT_Float_p(left) ? left : right,
        &LT_Number_class
    );
    return LT_NIL;
}

LT_Value LT_Number_subtract2(LT_Value left, LT_Value right){
    if ((LT_ExactComplexNumber_p(left) || LT_InexactComplexNumber_p(left)
            || LT_ExactComplexNumber_p(right) || LT_InexactComplexNumber_p(right))
        && LT_Number_value_p(left) && LT_Number_value_p(right)){
        return make_complex_from_values(
            LT_Number_subtract2(complex_real_part(left), complex_real_part(right)),
            LT_Number_subtract2(complex_imaginary_part(left), complex_imaginary_part(right))
        );
    }

    if (LT_RationalNumber_value_p(left) && LT_RationalNumber_value_p(right)){
        return checked_exact_subtract(left, right);
    }

    if (LT_Float_p(left) && LT_Float_p(right)){
        return LT_Float_new(LT_Float_value(left) - LT_Float_value(right));
    }

    if (LT_RationalNumber_value_p(left) && LT_Float_p(right)){
        return LT_Float_new(exact_to_double(left) - LT_Float_value(right));
    }
    if (LT_Float_p(left) && LT_RationalNumber_value_p(right)){
        return LT_Float_new(LT_Float_value(left) - exact_to_double(right));
    }

    LT_type_error(
        !LT_RationalNumber_value_p(left) && !LT_Float_p(left) ? left : right,
        &LT_Number_class
    );
    return LT_NIL;
}

LT_Value LT_Number_multiply2(LT_Value left, LT_Value right){
    if ((LT_ExactComplexNumber_p(left) || LT_InexactComplexNumber_p(left)
            || LT_ExactComplexNumber_p(right) || LT_InexactComplexNumber_p(right))
        && LT_Number_value_p(left) && LT_Number_value_p(right)){
        LT_Value a = complex_real_part(left);
        LT_Value b = complex_imaginary_part(left);
        LT_Value c = complex_real_part(right);
        LT_Value d = complex_imaginary_part(right);

        return make_complex_from_values(
            LT_Number_subtract2(LT_Number_multiply2(a, c), LT_Number_multiply2(b, d)),
            LT_Number_add2(LT_Number_multiply2(a, d), LT_Number_multiply2(b, c))
        );
    }

    if (LT_RationalNumber_value_p(left) && LT_RationalNumber_value_p(right)){
        return checked_exact_multiply(left, right);
    }

    if (LT_Float_p(left) && LT_Float_p(right)){
        return LT_Float_new(LT_Float_value(left) * LT_Float_value(right));
    }

    if (LT_RationalNumber_value_p(left) && LT_Float_p(right)){
        return LT_Float_new(exact_to_double(left) * LT_Float_value(right));
    }
    if (LT_Float_p(left) && LT_RationalNumber_value_p(right)){
        return LT_Float_new(LT_Float_value(left) * exact_to_double(right));
    }

    LT_type_error(
        !LT_RationalNumber_value_p(left) && !LT_Float_p(left) ? left : right,
        &LT_Number_class
    );
    return LT_NIL;
}

LT_Value LT_Number_divide2(LT_Value left, LT_Value right){
    if ((LT_ExactComplexNumber_p(left) || LT_InexactComplexNumber_p(left)
            || LT_ExactComplexNumber_p(right) || LT_InexactComplexNumber_p(right))
        && LT_Number_value_p(left) && LT_Number_value_p(right)){
        LT_Value a = complex_real_part(left);
        LT_Value b = complex_imaginary_part(left);
        LT_Value c = complex_real_part(right);
        LT_Value d = complex_imaginary_part(right);
        LT_Value denominator = LT_Number_add2(
            LT_Number_multiply2(c, c),
            LT_Number_multiply2(d, d)
        );

        return make_complex_from_values(
            LT_Number_divide2(
                LT_Number_add2(LT_Number_multiply2(a, c), LT_Number_multiply2(b, d)),
                denominator
            ),
            LT_Number_divide2(
                LT_Number_subtract2(LT_Number_multiply2(b, c), LT_Number_multiply2(a, d)),
                denominator
            )
        );
    }

    if (LT_RationalNumber_value_p(left) && LT_RationalNumber_value_p(right)){
        return checked_exact_divide(left, right);
    }

    if (LT_Float_p(left) && LT_Float_p(right)){
        double divisor = LT_Float_value(right);

        if (divisor == 0.0){
            LT_error("Division by zero");
        }
        return LT_Float_new(LT_Float_value(left) / divisor);
    }

    if (LT_RationalNumber_value_p(left) && LT_Float_p(right)){
        double divisor = LT_Float_value(right);

        if (divisor == 0.0){
            LT_error("Division by zero");
        }
        return LT_Float_new(exact_to_double(left) / divisor);
    }
    if (LT_Float_p(left) && LT_RationalNumber_value_p(right)){
        double divisor = exact_to_double(right);

        if (divisor == 0.0){
            LT_error("Division by zero");
        }
        return LT_Float_new(LT_Float_value(left) / divisor);
    }

    LT_type_error(
        !LT_RationalNumber_value_p(left) && !LT_Float_p(left) ? left : right,
        &LT_Number_class
    );
    return LT_NIL;
}

LT_Value LT_Number_negate(LT_Value value){
    if (LT_ExactComplexNumber_p(value) || LT_InexactComplexNumber_p(value)){
        return make_complex_from_values(
            LT_Number_negate(complex_real_part(value)),
            LT_Number_negate(complex_imaginary_part(value))
        );
    }
    if (LT_RationalNumber_value_p(value)){
        return checked_exact_negate(value);
    }
    if (LT_Float_p(value)){
        return LT_Float_new(-LT_Float_value(value));
    }

    LT_type_error(value, &LT_Number_class);
    return LT_NIL;
}

LT_Value LT_Number_abs(LT_Value value){
    double real;
    double imaginary;

    if (LT_RationalNumber_value_p(value)){
        return checked_exact_abs(value);
    }
    if (LT_Float_p(value)){
        return LT_Float_new(fabs(LT_Float_value(value)));
    }

    complex_number_to_doubles(value, &real, &imaginary);
    return LT_Float_new(hypot(real, imaginary));
}

LT_Value LT_Number_phase(LT_Value value){
    double real;
    double imaginary;

    if (LT_RealNumber_value_p(value)){
        real = LT_Number_to_double(value);
        return LT_Float_new(atan2(0.0, real));
    }

    complex_number_to_doubles(value, &real, &imaginary);
    return LT_Float_new(atan2(imaginary, real));
}

LT_Value LT_Number_floor(LT_Value value){
    if (LT_RationalNumber_value_p(value)){
        return checked_exact_floor(value);
    }
    if (LT_Float_p(value)){
        return double_to_integer(floor(LT_Float_value(value)));
    }

    LT_type_error(value, &LT_RealNumber_class);
    return LT_NIL;
}

LT_Value LT_Number_truncate(LT_Value value){
    if (LT_RationalNumber_value_p(value)){
        return checked_exact_truncate(value);
    }
    if (LT_Float_p(value)){
        return double_to_integer(trunc(LT_Float_value(value)));
    }

    LT_type_error(value, &LT_RealNumber_class);
    return LT_NIL;
}

LT_Value LT_Number_ceiling(LT_Value value){
    if (LT_RationalNumber_value_p(value)){
        return checked_exact_ceiling(value);
    }
    if (LT_Float_p(value)){
        return double_to_integer(ceil(LT_Float_value(value)));
    }

    LT_type_error(value, &LT_RealNumber_class);
    return LT_NIL;
}

LT_Value LT_Number_round(LT_Value value){
    if (LT_RationalNumber_value_p(value)){
        return checked_exact_round(value);
    }
    if (LT_Float_p(value)){
        return double_to_integer(round(LT_Float_value(value)));
    }

    LT_type_error(value, &LT_RealNumber_class);
    return LT_NIL;
}

LT_Value LT_Number_sqrt(LT_Value value){
    double real;
    double imaginary;
    double magnitude;
    double result_real;
    double result_imaginary;

    if (LT_RealNumber_value_p(value)){
        double number = LT_Number_to_double(value);

        if (number >= 0.0){
            return LT_Float_new(sqrt(number));
        }
        return LT_InexactComplexNumber_new(0.0, sqrt(-number));
    }

    complex_number_to_doubles(value, &real, &imaginary);
    magnitude = hypot(real, imaginary);
    result_real = sqrt((magnitude + real) / 2.0);
    result_imaginary = sqrt((magnitude - real) / 2.0);

    if (imaginary < 0.0){
        result_imaginary = -result_imaginary;
    }
    return LT_InexactComplexNumber_new(result_real, result_imaginary);
}

LT_Value LT_Number_min2(LT_Value left, LT_Value right){
    return compare_real_numbers(left, right) <= 0 ? left : right;
}

LT_Value LT_Number_max2(LT_Value left, LT_Value right){
    return compare_real_numbers(left, right) >= 0 ? left : right;
}

bool LT_Number_zero_p(LT_Value value){
    if (!LT_Number_value_p(value)){
        LT_type_error(value, &LT_Number_class);
    }
    return LT_Number_equal_p(value, LT_SmallInteger_new(0));
}

bool LT_Number_positive_p(LT_Value value){
    return compare_real_numbers(value, LT_SmallInteger_new(0)) > 0;
}

bool LT_Number_negative_p(LT_Value value){
    return compare_real_numbers(value, LT_SmallInteger_new(0)) < 0;
}

LT_Value LT_Number_sin(LT_Value value){
    double real;
    double imaginary;

    if (LT_RealNumber_value_p(value)){
        return LT_Float_new(sin(LT_Number_to_double(value)));
    }

    complex_number_to_doubles(value, &real, &imaginary);
    return LT_InexactComplexNumber_new(
        sin(real) * cosh(imaginary),
        cos(real) * sinh(imaginary)
    );
}

LT_Value LT_Number_cos(LT_Value value){
    double real;
    double imaginary;

    if (LT_RealNumber_value_p(value)){
        return LT_Float_new(cos(LT_Number_to_double(value)));
    }

    complex_number_to_doubles(value, &real, &imaginary);
    return LT_InexactComplexNumber_new(
        cos(real) * cosh(imaginary),
        -sin(real) * sinh(imaginary)
    );
}

LT_Value LT_Number_tan(LT_Value value){
    if (LT_RealNumber_value_p(value)){
        return LT_Float_new(tan(LT_Number_to_double(value)));
    }

    return LT_Number_divide2(LT_Number_sin(value), LT_Number_cos(value));
}

LT_Value LT_Number_log(LT_Value value){
    double real;
    double imaginary;

    if (LT_RealNumber_value_p(value)){
        double number = LT_Number_to_double(value);

        if (number >= 0.0){
            return LT_Float_new(log(number));
        }
        return LT_InexactComplexNumber_new(log(-number), atan2(0.0, number));
    }

    complex_number_to_doubles(value, &real, &imaginary);
    return LT_InexactComplexNumber_new(
        log(hypot(real, imaginary)),
        atan2(imaginary, real)
    );
}

LT_Value LT_Number_exp(LT_Value value){
    double real;
    double imaginary;

    if (LT_RealNumber_value_p(value)){
        return LT_Float_new(exp(LT_Number_to_double(value)));
    }

    complex_number_to_doubles(value, &real, &imaginary);
    return LT_InexactComplexNumber_new(
        exp(real) * cos(imaginary),
        exp(real) * sin(imaginary)
    );
}

LT_Value LT_Number_expt(LT_Value base, LT_Value exponent){
    int exponent_is_exact_integer = 0;
    LT_Value exponent_integer = value_to_exact_integer(exponent, &exponent_is_exact_integer);

    if (exponent_is_exact_integer && LT_Number_value_p(base)){
        return number_integer_power(base, exponent_integer);
    }

    if (!LT_Number_value_p(base)){
        LT_type_error(base, &LT_ComplexNumber_class);
    }
    if (!LT_Number_value_p(exponent)){
        LT_type_error(exponent, &LT_ComplexNumber_class);
    }

    if (LT_RealNumber_value_p(base) && LT_RealNumber_value_p(exponent)){
        double base_value = LT_Number_to_double(base);

        if (base_value >= 0.0){
            return LT_Float_new(pow(base_value, LT_Number_to_double(exponent)));
        }
    }

    return LT_Number_exp(LT_Number_multiply2(exponent, LT_Number_log(base)));
}
