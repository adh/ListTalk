/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/BigInteger.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/Fraction.h>
#include <ListTalk/classes/SmallFraction.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/error.h>

#include <stdbool.h>
#include <math.h>
#include <string.h>

typedef __int128 LT_Int128;
typedef unsigned __int128 LT_UInt128;

struct LT_Number_s {
    LT_Object base;
};

struct LT_BigInteger_s {
    LT_Object base;
    LT_Int128 value;
};

struct LT_Fraction_s {
    LT_Object base;
    LT_Value numerator;
    LT_Value denominator;
};

typedef struct LT_ExactRational_s {
    LT_Int128 numerator;
    LT_UInt128 denominator;
} LT_ExactRational;

static size_t hash_uint64(uint64_t value){
    value ^= value >> 33;
    value *= UINT64_C(0xff51afd7ed558ccd);
    value ^= value >> 33;
    value *= UINT64_C(0xc4ceb9fe1a85ec53);
    value ^= value >> 33;
    return (size_t)value;
}

static int Number_class_equal_p(LT_Value left, LT_Value right){
    return LT_Number_equal_p(left, right);
}

static LT_UInt128 uabs_i128(LT_Int128 value){
    if (value < 0){
        return (LT_UInt128)(-(value + 1)) + 1;
    }
    return (LT_UInt128)value;
}

static LT_Int128 i128_from_u128(LT_UInt128 value){
    LT_UInt128 min_magnitude = (LT_UInt128)1 << 127;

    if (value == min_magnitude){
        return (((LT_Int128)1) << 126) * (-2);
    }
    return (LT_Int128)value;
}

static LT_Int128 big_integer_value(LT_Value value){
    return LT_BigInteger_from_value(value)->value;
}

static bool value_is_exact_integer(LT_Value value){
    return LT_Value_is_fixnum(value) || LT_BigInteger_p(value);
}

static bool value_is_exact_number(LT_Value value){
    return value_is_exact_integer(value)
        || LT_SmallFraction_p(value)
        || LT_Fraction_p(value);
}

static LT_Int128 integer_value_i128(LT_Value value){
    if (LT_Value_is_fixnum(value)){
        return (LT_Int128)LT_SmallInteger_value(value);
    }
    if (LT_BigInteger_p(value)){
        return big_integer_value(value);
    }

    LT_type_error(value, &LT_Number_class);
    return 0;
}

static LT_UInt128 gcd_u128(LT_UInt128 left, LT_UInt128 right){
    while (right != 0){
        LT_UInt128 remainder = left % right;
        left = right;
        right = remainder;
    }
    return left;
}

static LT_Value make_integer_i128(LT_Int128 value){
    LT_BigInteger* integer;

    if (value >= (LT_Int128)LT_VALUE_FIXNUM_MIN
        && value <= (LT_Int128)LT_VALUE_FIXNUM_MAX){
        return LT_SmallInteger_new((int64_t)value);
    }

    integer = LT_Class_ALLOC(LT_BigInteger);
    integer->value = value;
    return (LT_Value)(uintptr_t)integer;
}

static LT_Value make_fraction_i128(LT_Int128 numerator, LT_UInt128 denominator){
    LT_UInt128 divisor;
    LT_Fraction* fraction;
    LT_Value canonical_numerator;
    LT_Value canonical_denominator;

    if (denominator == 0){
        LT_error("Division by zero");
    }
    if (numerator == 0){
        return LT_SmallInteger_new(0);
    }

    divisor = gcd_u128(uabs_i128(numerator), denominator);
    numerator /= i128_from_u128(divisor);
    denominator /= divisor;

    if (denominator == 1){
        return make_integer_i128(numerator);
    }

    if (denominator <= (LT_UInt128)LT_SMALL_FRACTION_DENOMINATOR_MAX
        && numerator >= (LT_Int128)LT_SMALL_FRACTION_NUMERATOR_MIN
        && numerator <= (LT_Int128)LT_SMALL_FRACTION_NUMERATOR_MAX){
        return LT_SmallFraction_new((int64_t)numerator, (uint32_t)denominator);
    }

    canonical_numerator = make_integer_i128(numerator);
    canonical_denominator = make_integer_i128((LT_Int128)denominator);
    fraction = LT_Class_ALLOC(LT_Fraction);
    fraction->numerator = canonical_numerator;
    fraction->denominator = canonical_denominator;
    return (LT_Value)(uintptr_t)fraction;
}

static LT_ExactRational exact_rational_from_value(LT_Value value){
    LT_ExactRational result;

    if (LT_Value_is_fixnum(value)){
        result.numerator = (LT_Int128)LT_SmallInteger_value(value);
        result.denominator = 1;
        return result;
    }
    if (LT_BigInteger_p(value)){
        result.numerator = big_integer_value(value);
        result.denominator = 1;
        return result;
    }
    if (LT_SmallFraction_p(value)){
        result.numerator = (LT_Int128)LT_SmallFraction_numerator(value);
        result.denominator = (LT_UInt128)LT_SmallFraction_denominator(value);
        return result;
    }
    if (LT_Fraction_p(value)){
        result.numerator = integer_value_i128(LT_Fraction_numerator(value));
        result.denominator = (LT_UInt128)integer_value_i128(
            LT_Fraction_denominator(value)
        );
        return result;
    }

    LT_type_error(value, &LT_Number_class);
    result.numerator = 0;
    result.denominator = 1;
    return result;
}

static double exact_to_double(LT_Value value){
    LT_ExactRational rational = exact_rational_from_value(value);
    return (double)rational.numerator / (double)rational.denominator;
}

static LT_Value checked_exact_add(LT_Value left, LT_Value right){
    LT_ExactRational lhs = exact_rational_from_value(left);
    LT_ExactRational rhs = exact_rational_from_value(right);
    LT_Int128 numerator;
    LT_UInt128 denominator;
    LT_Int128 left_scaled;
    LT_Int128 right_scaled;

    if (__builtin_mul_overflow(lhs.numerator, i128_from_u128(rhs.denominator), &left_scaled)
        || __builtin_mul_overflow(rhs.numerator, i128_from_u128(lhs.denominator), &right_scaled)
        || __builtin_add_overflow(left_scaled, right_scaled, &numerator)){
        LT_error("Exact arithmetic overflow");
    }

    denominator = lhs.denominator * rhs.denominator;
    return make_fraction_i128(numerator, denominator);
}

static LT_Value checked_exact_subtract(LT_Value left, LT_Value right){
    LT_ExactRational lhs = exact_rational_from_value(left);
    LT_ExactRational rhs = exact_rational_from_value(right);
    LT_Int128 numerator;
    LT_UInt128 denominator;
    LT_Int128 left_scaled;
    LT_Int128 right_scaled;

    if (__builtin_mul_overflow(lhs.numerator, i128_from_u128(rhs.denominator), &left_scaled)
        || __builtin_mul_overflow(rhs.numerator, i128_from_u128(lhs.denominator), &right_scaled)
        || __builtin_sub_overflow(left_scaled, right_scaled, &numerator)){
        LT_error("Exact arithmetic overflow");
    }

    denominator = lhs.denominator * rhs.denominator;
    return make_fraction_i128(numerator, denominator);
}

static LT_Value checked_exact_multiply(LT_Value left, LT_Value right){
    LT_ExactRational lhs = exact_rational_from_value(left);
    LT_ExactRational rhs = exact_rational_from_value(right);
    LT_Int128 numerator;
    LT_UInt128 denominator;

    if (__builtin_mul_overflow(lhs.numerator, rhs.numerator, &numerator)){
        LT_error("Exact arithmetic overflow");
    }
    denominator = lhs.denominator * rhs.denominator;
    return make_fraction_i128(numerator, denominator);
}

static LT_Value checked_exact_divide(LT_Value left, LT_Value right){
    LT_ExactRational lhs = exact_rational_from_value(left);
    LT_ExactRational rhs = exact_rational_from_value(right);
    LT_Int128 numerator;
    LT_UInt128 denominator;
    LT_Int128 rhs_numerator = rhs.numerator;

    if (rhs_numerator == 0){
        LT_error("Division by zero");
    }
    if (rhs_numerator < 0){
        lhs.numerator = -lhs.numerator;
        rhs_numerator = -rhs_numerator;
    }

    if (__builtin_mul_overflow(lhs.numerator, i128_from_u128(rhs.denominator), &numerator)){
        LT_error("Exact arithmetic overflow");
    }
    denominator = lhs.denominator * (LT_UInt128)rhs_numerator;
    return make_fraction_i128(numerator, denominator);
}

static LT_Value checked_exact_negate(LT_Value value){
    LT_ExactRational rational = exact_rational_from_value(value);
    LT_Int128 numerator;

    if (__builtin_sub_overflow((LT_Int128)0, rational.numerator, &numerator)){
        LT_error("Exact arithmetic overflow");
    }
    return make_fraction_i128(numerator, rational.denominator);
}

static size_t exact_number_hash(LT_Value value){
    LT_ExactRational rational = exact_rational_from_value(value);

    if (rational.denominator == 1){
        return hash_uint64((uint64_t)rational.numerator)
            ^ (hash_uint64((uint64_t)(rational.numerator >> 64)) << 1);
    }

    return hash_uint64((uint64_t)rational.numerator)
        ^ (hash_uint64((uint64_t)(rational.numerator >> 64)) << 1)
        ^ (hash_uint64((uint64_t)rational.denominator) << 2)
        ^ (hash_uint64((uint64_t)(rational.denominator >> 64)) << 3);
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

static bool parse_integer_token_i128(const char* token, LT_Int128* value){
    const char* cursor = token;
    LT_UInt128 magnitude = 0;
    LT_UInt128 limit;
    int negative = 0;

    if (*cursor == '+' || *cursor == '-'){
        negative = *cursor == '-';
        cursor++;
    }

    if (*cursor == '\0'){
        return false;
    }

    limit = negative ? ((LT_UInt128)1 << 127) : (((LT_UInt128)1 << 127) - 1);
    while (*cursor != '\0'){
        unsigned char ch = (unsigned char)*cursor;

        if (ch < '0' || ch > '9'){
            return false;
        }
        if (magnitude > (limit - (LT_UInt128)(ch - '0')) / 10){
            LT_error("Integer literal out of range");
        }
        magnitude = magnitude * 10 + (LT_UInt128)(ch - '0');
        cursor++;
    }

    if (!negative){
        *value = (LT_Int128)magnitude;
        return true;
    }

    if (magnitude == ((LT_UInt128)1 << 127)){
        *value = (((LT_Int128)1) << 126) * (-2);
        return true;
    }

    *value = -(LT_Int128)magnitude;
    return true;
}

int LT_Number_parse_integer_token(const char* token, LT_Value* value){
    LT_Int128 parsed;

    if (!parse_integer_token_i128(token, &parsed)){
        return 0;
    }

    *value = make_integer_i128(parsed);
    return 1;
}

int LT_Number_parse_fraction_token(const char* token, LT_Value* value){
    const char* slash = strchr(token, '/');
    char* numerator_token;
    char* denominator_token;
    size_t numerator_length;
    LT_Int128 numerator;
    LT_Int128 denominator;

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

    if (!parse_integer_token_i128(numerator_token, &numerator)
        || !parse_integer_token_i128(denominator_token, &denominator)){
        return 0;
    }

    if (denominator == 0){
        LT_error("Division by zero");
    }
    if (denominator < 0){
        numerator = -numerator;
        denominator = -denominator;
    }

    *value = make_fraction_i128(numerator, (LT_UInt128)denominator);
    return 1;
}

char* LT_BigInteger_to_decimal_cstr(LT_Value value){
    LT_Int128 integer;
    LT_UInt128 magnitude;
    char* buffer;
    size_t index = 0;
    int negative = 0;

    if (!LT_BigInteger_p(value)){
        LT_type_error(value, &LT_BigInteger_class);
    }

    integer = big_integer_value(value);
    magnitude = uabs_i128(integer);
    negative = integer < 0;
    buffer = GC_MALLOC_ATOMIC(64);

    do {
        buffer[index++] = (char)('0' + (int)(magnitude % 10));
        magnitude /= 10;
    } while (magnitude != 0);

    if (negative){
        buffer[index++] = '-';
    }
    buffer[index] = '\0';

    for (size_t left = 0, right = index - 1; left < right; left++, right--){
        char tmp = buffer[left];
        buffer[left] = buffer[right];
        buffer[right] = tmp;
    }

    return buffer;
}

LT_Value LT_BigInteger_new_from_digits(const char* digits){
    LT_Value value;

    if (!LT_Number_parse_integer_token(digits, &value)){
        LT_error("Invalid integer literal");
    }
    return value;
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
        return lhs.numerator == rhs.numerator && lhs.denominator == rhs.denominator;
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
