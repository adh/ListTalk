/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "BigInteger_internal.h"

#include <ListTalk/classes/Number.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/error.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct LT_BigInteger_s {
    LT_Object base;
    uint8_t negative;
    uint8_t reserved[3];
    size_t limb_count;
    uint32_t limbs[];
};

typedef struct LT_IntegerRef_s {
    int negative;
    size_t limb_count;
    const uint32_t* limbs;
    uint32_t small[2];
} LT_IntegerRef;


static size_t trim_limb_count(size_t limb_count, const uint32_t* limbs){
    while (limb_count > 0 && limbs[limb_count - 1] == 0){
        limb_count--;
    }
    return limb_count;
}

static void integer_ref_init(LT_IntegerRef* ref, LT_Value value){
    if (LT_Value_is_fixnum(value)){
        int64_t small = LT_SmallInteger_value(value);
        uint64_t magnitude;

        ref->negative = small < 0;
        magnitude = ref->negative ? (uint64_t)(-(small + 1)) + 1 : (uint64_t)small;
        if (magnitude == 0){
            ref->limb_count = 0;
        } else if ((magnitude >> 32) != 0){
            ref->small[0] = (uint32_t)magnitude;
            ref->small[1] = (uint32_t)(magnitude >> 32);
            ref->limb_count = 2;
        } else {
            ref->small[0] = (uint32_t)magnitude;
            ref->limb_count = 1;
        }
        ref->limbs = ref->small;
        return;
    }

    if (!LT_BigInteger_p(value)){
        LT_type_error(value, &LT_BigInteger_class);
    }

    ref->negative = LT_BigInteger_from_value(value)->negative != 0;
    ref->limb_count = LT_BigInteger_from_value(value)->limb_count;
    ref->limbs = LT_BigInteger_from_value(value)->limbs;
}

static int compare_abs_limbs(
    size_t left_count,
    const uint32_t* left_limbs,
    size_t right_count,
    const uint32_t* right_limbs
){
    size_t index;

    left_count = trim_limb_count(left_count, left_limbs);
    right_count = trim_limb_count(right_count, right_limbs);

    if (left_count < right_count){
        return -1;
    }
    if (left_count > right_count){
        return 1;
    }

    for (index = left_count; index > 0; index--){
        uint32_t left_limb = left_limbs[index - 1];
        uint32_t right_limb = right_limbs[index - 1];

        if (left_limb < right_limb){
            return -1;
        }
        if (left_limb > right_limb){
            return 1;
        }
    }

    return 0;
}

static uint32_t* copy_limbs(size_t limb_count, const uint32_t* limbs){
    uint32_t* copy;

    if (limb_count == 0){
        return NULL;
    }

    copy = GC_MALLOC_ATOMIC(sizeof(uint32_t) * limb_count);
    memcpy(copy, limbs, sizeof(uint32_t) * limb_count);
    return copy;
}

static uint32_t* add_abs_limbs(
    size_t left_count,
    const uint32_t* left_limbs,
    size_t right_count,
    const uint32_t* right_limbs,
    size_t* result_count
){
    size_t index;
    size_t capacity = (left_count > right_count ? left_count : right_count) + 1;
    uint32_t* result = GC_MALLOC_ATOMIC(sizeof(uint32_t) * capacity);
    uint64_t carry = 0;

    for (index = 0; index < capacity - 1; index++){
        uint64_t sum = carry;

        if (index < left_count){
            sum += left_limbs[index];
        }
        if (index < right_count){
            sum += right_limbs[index];
        }

        result[index] = (uint32_t)sum;
        carry = sum >> 32;
    }

    result[capacity - 1] = (uint32_t)carry;
    *result_count = trim_limb_count(capacity, result);
    return result;
}

static uint32_t* subtract_abs_limbs(
    size_t left_count,
    const uint32_t* left_limbs,
    size_t right_count,
    const uint32_t* right_limbs,
    size_t* result_count
){
    size_t index;
    uint32_t* result = GC_MALLOC_ATOMIC(sizeof(uint32_t) * left_count);
    uint64_t borrow = 0;

    for (index = 0; index < left_count; index++){
        uint64_t left_limb = left_limbs[index];
        uint64_t right_limb = index < right_count ? right_limbs[index] : 0;
        uint64_t subtrahend = right_limb + borrow;

        if (left_limb < subtrahend){
            result[index] = (uint32_t)((UINT64_C(1) << 32) + left_limb - subtrahend);
            borrow = 1;
        } else {
            result[index] = (uint32_t)(left_limb - subtrahend);
            borrow = 0;
        }
    }

    *result_count = trim_limb_count(left_count, result);
    return result;
}

static void subtract_abs_inplace(
    uint32_t* left_limbs,
    size_t* left_count,
    size_t right_count,
    const uint32_t* right_limbs
){
    size_t index;
    uint64_t borrow = 0;

    for (index = 0; index < *left_count; index++){
        uint64_t left_limb = left_limbs[index];
        uint64_t right_limb = index < right_count ? right_limbs[index] : 0;
        uint64_t subtrahend = right_limb + borrow;

        if (left_limb < subtrahend){
            left_limbs[index] = (uint32_t)((UINT64_C(1) << 32) + left_limb - subtrahend);
            borrow = 1;
        } else {
            left_limbs[index] = (uint32_t)(left_limb - subtrahend);
            borrow = 0;
        }
    }

    *left_count = trim_limb_count(*left_count, left_limbs);
}

static uint32_t* multiply_abs_limbs(
    size_t left_count,
    const uint32_t* left_limbs,
    size_t right_count,
    const uint32_t* right_limbs,
    size_t* result_count
){
    size_t left_index;
    size_t right_index;
    size_t capacity = left_count + right_count;
    uint32_t* result;

    if (left_count == 0 || right_count == 0){
        *result_count = 0;
        return NULL;
    }

    result = GC_MALLOC_ATOMIC(sizeof(uint32_t) * capacity);
    memset(result, 0, sizeof(uint32_t) * capacity);

    for (left_index = 0; left_index < left_count; left_index++){
        uint64_t carry = 0;

        for (right_index = 0; right_index < right_count; right_index++){
            uint64_t product = (uint64_t)left_limbs[left_index]
                * (uint64_t)right_limbs[right_index]
                + result[left_index + right_index]
                + carry;

            result[left_index + right_index] = (uint32_t)product;
            carry = product >> 32;
        }

        for (right_index = left_index + right_count; carry != 0; right_index++){
            uint64_t sum = (uint64_t)result[right_index] + carry;
            result[right_index] = (uint32_t)sum;
            carry = sum >> 32;
        }
    }

    *result_count = trim_limb_count(capacity, result);
    return result;
}

static size_t bit_length_abs(size_t limb_count, const uint32_t* limbs){
    uint32_t top;
    unsigned int bits = 0;

    limb_count = trim_limb_count(limb_count, limbs);
    if (limb_count == 0){
        return 0;
    }

    top = limbs[limb_count - 1];
    while (top != 0){
        bits++;
        top >>= 1;
    }

    return (limb_count - 1) * 32 + bits;
}

static uint32_t* shift_left_abs(
    size_t limb_count,
    const uint32_t* limbs,
    size_t shift_bits,
    size_t* result_count
){
    size_t word_shift = shift_bits / 32;
    size_t bit_shift = shift_bits % 32;
    size_t index;
    size_t capacity;
    uint32_t* result;

    if (limb_count == 0){
        *result_count = 0;
        return NULL;
    }

    capacity = limb_count + word_shift + (bit_shift != 0 ? 1 : 0);
    result = GC_MALLOC_ATOMIC(sizeof(uint32_t) * capacity);
    memset(result, 0, sizeof(uint32_t) * capacity);

    if (bit_shift == 0){
        memcpy(result + word_shift, limbs, sizeof(uint32_t) * limb_count);
        *result_count = trim_limb_count(capacity, result);
        return result;
    }

    for (index = 0; index < limb_count; index++){
        uint64_t shifted = (uint64_t)limbs[index] << bit_shift;
        result[index + word_shift] |= (uint32_t)shifted;
        result[index + word_shift + 1] = (uint32_t)(shifted >> 32);
    }

    *result_count = trim_limb_count(capacity, result);
    return result;
}

static void shift_right_one_inplace(uint32_t* limbs, size_t* limb_count){
    size_t index;
    uint32_t carry = 0;

    if (*limb_count == 0){
        return;
    }

    for (index = *limb_count; index > 0; index--){
        uint32_t next_carry = limbs[index - 1] & 1;
        limbs[index - 1] = (limbs[index - 1] >> 1) | (carry << 31);
        carry = next_carry;
    }

    *limb_count = trim_limb_count(*limb_count, limbs);
}

static int limbs_fit_fixnum(
    int negative,
    size_t limb_count,
    const uint32_t* limbs,
    int64_t* result
){
    uint64_t magnitude = 0;

    limb_count = trim_limb_count(limb_count, limbs);
    if (limb_count == 0){
        *result = 0;
        return 1;
    }
    if (limb_count > 2){
        return 0;
    }

    magnitude = limbs[0];
    if (limb_count == 2){
        magnitude |= (uint64_t)limbs[1] << 32;
    }

    if (!negative){
        if (magnitude > (uint64_t)LT_VALUE_FIXNUM_MAX){
            return 0;
        }
        *result = (int64_t)magnitude;
        return 1;
    }

    if (magnitude > (uint64_t)(-(LT_VALUE_FIXNUM_MIN + 1)) + 1){
        return 0;
    }

    *result = -(int64_t)magnitude;
    return 1;
}

static LT_Value make_integer_from_limbs(
    int negative,
    size_t limb_count,
    const uint32_t* limbs
){
    LT_BigInteger* integer;
    int64_t small_value;

    limb_count = trim_limb_count(limb_count, limbs);
    if (limb_count == 0){
        return LT_SmallInteger_new(0);
    }

    if (limbs_fit_fixnum(negative, limb_count, limbs, &small_value)){
        return LT_SmallInteger_new(small_value);
    }

    integer = LT_Class_ALLOC_FLEXIBLE(LT_BigInteger, sizeof(uint32_t) * limb_count);
    integer->negative = negative ? 1 : 0;
    integer->reserved[0] = 0;
    integer->reserved[1] = 0;
    integer->reserved[2] = 0;
    integer->limb_count = limb_count;
    memcpy(integer->limbs, limbs, sizeof(uint32_t) * limb_count);
    return (LT_Value)(uintptr_t)integer;
}

static void divide_abs_values(
    size_t dividend_count,
    const uint32_t* dividend_limbs,
    size_t divisor_count,
    const uint32_t* divisor_limbs,
    size_t* quotient_count,
    uint32_t** quotient_limbs,
    size_t* remainder_count,
    uint32_t** remainder_limbs
){
    size_t shift;
    size_t shifted_count;
    uint32_t* shifted_divisor;
    uint32_t* remainder;
    uint32_t* quotient;
    size_t quotient_capacity;

    if (divisor_count == 0){
        LT_error("Division by zero");
    }

    dividend_count = trim_limb_count(dividend_count, dividend_limbs);
    divisor_count = trim_limb_count(divisor_count, divisor_limbs);

    if (divisor_count == 0){
        LT_error("Division by zero");
    }

    /* Single-limb divisor: O(n) linear scan with 64-bit arithmetic */
    if (divisor_count == 1){
        uint64_t d = divisor_limbs[0];
        uint64_t rem = 0;
        size_t i;

        if (dividend_count == 0){
            *quotient_count = 0;
            *quotient_limbs = NULL;
            *remainder_count = 0;
            *remainder_limbs = NULL;
            return;
        }

        quotient = GC_MALLOC_ATOMIC(sizeof(uint32_t) * dividend_count);
        for (i = dividend_count; i > 0; i--){
            uint64_t cur = (rem << 32) | dividend_limbs[i - 1];
            quotient[i - 1] = (uint32_t)(cur / d);
            rem = cur % d;
        }
        *quotient_count = trim_limb_count(dividend_count, quotient);
        *quotient_limbs = quotient;

        if (rem != 0){
            uint32_t* rem_limbs = GC_MALLOC_ATOMIC(sizeof(uint32_t));
            rem_limbs[0] = (uint32_t)rem;
            *remainder_count = 1;
            *remainder_limbs = rem_limbs;
        } else {
            *remainder_count = 0;
            *remainder_limbs = NULL;
        }
        return;
    }

    if (compare_abs_limbs(dividend_count, dividend_limbs, divisor_count, divisor_limbs) < 0){
        *quotient_count = 0;
        *quotient_limbs = NULL;
        *remainder_count = dividend_count;
        *remainder_limbs = copy_limbs(dividend_count, dividend_limbs);
        return;
    }

    shift = bit_length_abs(dividend_count, dividend_limbs)
        - bit_length_abs(divisor_count, divisor_limbs);
    shifted_divisor = shift_left_abs(divisor_count, divisor_limbs, shift, &shifted_count);
    remainder = copy_limbs(dividend_count, dividend_limbs);
    *remainder_count = trim_limb_count(dividend_count, dividend_limbs);
    quotient_capacity = shift / 32 + 1;
    quotient = GC_MALLOC_ATOMIC(sizeof(uint32_t) * quotient_capacity);
    memset(quotient, 0, sizeof(uint32_t) * quotient_capacity);

    while (1){
        if (compare_abs_limbs(*remainder_count, remainder, shifted_count, shifted_divisor) >= 0){
            subtract_abs_inplace(remainder, remainder_count, shifted_count, shifted_divisor);
            quotient[shift / 32] |= (uint32_t)1 << (shift % 32);
        }
        if (shift == 0){
            break;
        }
        shift_right_one_inplace(shifted_divisor, &shifted_count);
        shift--;
    }

    *quotient_count = trim_limb_count(quotient_capacity, quotient);
    *quotient_limbs = quotient;
    *remainder_limbs = remainder;
}

static void BigInteger_debugPrintOn(LT_Value value, FILE* stream){
    char* text = LT_BigInteger_to_decimal_cstr(value);
    fputs(text, stream);
}

LT_DEFINE_CLASS(LT_BigInteger) {
    .superclass = &LT_Number_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "BigInteger",
    .instance_size = sizeof(LT_BigInteger),
    .class_flags = LT_CLASS_FLAG_IMMUTABLE | LT_CLASS_FLAG_SCALAR,
    .debugPrintOn = BigInteger_debugPrintOn,
};

bool LT_Integer_p(LT_Value value){
    return LT_Value_is_fixnum(value) || LT_BigInteger_p(value);
}

bool LT_Integer_is_zero(LT_Value value){
    if (LT_Value_is_fixnum(value)){
        return LT_SmallInteger_value(value) == 0;
    }
    LT_IntegerRef ref;
    integer_ref_init(&ref, value);
    return ref.limb_count == 0;
}

bool LT_Integer_negative_p(LT_Value value){
    if (LT_Value_is_fixnum(value)){
        return LT_SmallInteger_value(value) < 0;
    }
    LT_IntegerRef ref;
    integer_ref_init(&ref, value);
    return ref.negative && ref.limb_count != 0;
}

int LT_Integer_compare(LT_Value left, LT_Value right){
    LT_IntegerRef lhs;
    integer_ref_init(&lhs, left);
    LT_IntegerRef rhs;
    integer_ref_init(&rhs, right);
    int abs_compare;

    if (lhs.limb_count == 0 && rhs.limb_count == 0){
        return 0;
    }
    if (lhs.negative != rhs.negative){
        return lhs.negative ? -1 : 1;
    }

    abs_compare = compare_abs_limbs(lhs.limb_count, lhs.limbs, rhs.limb_count, rhs.limbs);
    return lhs.negative ? -abs_compare : abs_compare;
}

size_t LT_Integer_hash(LT_Value value){
    if (LT_Value_is_fixnum(value)){
        return hash_uint64((uint64_t)LT_SmallInteger_value(value));
    }
    LT_IntegerRef ref;
    integer_ref_init(&ref, value);
    size_t hash = ref.negative ? hash_uint64(UINT64_C(0x9e3779b97f4a7c15)) : 0;
    size_t index;

    if (ref.limb_count == 0){
        return hash_uint64(0);
    }

    for (index = 0; index < ref.limb_count; index++){
        hash ^= hash_uint64(ref.limbs[index]) + (hash << 6) + (hash >> 2);
    }
    return hash;
}

LT_Value LT_Integer_abs(LT_Value value){
    if (LT_Value_is_fixnum(value)){
        int64_t v = LT_SmallInteger_value(value);
        if (v >= 0){
            return value;
        }
        /* INT64_MIN cannot be negated as a fixnum — it needs a BigInteger.
           But LT_VALUE_FIXNUM_MIN == -(2^55), so -LT_VALUE_FIXNUM_MIN == 2^55
           which exceeds LT_VALUE_FIXNUM_MAX (2^55-1). */
        if (v == LT_VALUE_FIXNUM_MIN){
            uint32_t limbs[2];
            uint64_t magnitude = (uint64_t)LT_VALUE_FIXNUM_MAX + 1;
            limbs[0] = (uint32_t)magnitude;
            limbs[1] = (uint32_t)(magnitude >> 32);
            return make_integer_from_limbs(0, 2, limbs);
        }
        return LT_SmallInteger_new(-v);
    }
    LT_IntegerRef ref;
    integer_ref_init(&ref, value);

    if (!ref.negative){
        return value;
    }

    return make_integer_from_limbs(0, ref.limb_count, ref.limbs);
}

LT_Value LT_Integer_negate(LT_Value value){
    if (LT_Value_is_fixnum(value)){
        int64_t v = LT_SmallInteger_value(value);
        if (v == 0){
            return value;
        }
        if (v == LT_VALUE_FIXNUM_MIN){
            uint32_t limbs[2];
            uint64_t magnitude = (uint64_t)LT_VALUE_FIXNUM_MAX + 1;
            limbs[0] = (uint32_t)magnitude;
            limbs[1] = (uint32_t)(magnitude >> 32);
            return make_integer_from_limbs(0, 2, limbs);
        }
        if (LT_SmallInteger_in_range(-v)){
            return LT_SmallInteger_new(-v);
        }
    }
    LT_IntegerRef ref;
    integer_ref_init(&ref, value);

    if (ref.limb_count == 0){
        return value;
    }

    return make_integer_from_limbs(!ref.negative, ref.limb_count, ref.limbs);
}

LT_Value LT_Integer_add(LT_Value left, LT_Value right){
    LT_IntegerRef lhs;
    integer_ref_init(&lhs, left);
    LT_IntegerRef rhs;
    integer_ref_init(&rhs, right);
    size_t result_count;
    uint32_t* result_limbs;
    int compare;

    if (lhs.limb_count <= 2 && rhs.limb_count <= 2){
        int64_t a, b;
        if (limbs_fit_fixnum(lhs.negative, lhs.limb_count, lhs.limbs, &a)
            && limbs_fit_fixnum(rhs.negative, rhs.limb_count, rhs.limbs, &b)){
            /* Both fit in int64_t; 56-bit fixnums can't overflow int64_t on add */
            int64_t sum = a + b;
            if (LT_SmallInteger_in_range(sum)){
                return LT_SmallInteger_new(sum);
            }
        }
    }

    if (lhs.negative == rhs.negative){
        result_limbs = add_abs_limbs(
            lhs.limb_count,
            lhs.limbs,
            rhs.limb_count,
            rhs.limbs,
            &result_count
        );
        return make_integer_from_limbs(lhs.negative, result_count, result_limbs);
    }

    compare = compare_abs_limbs(lhs.limb_count, lhs.limbs, rhs.limb_count, rhs.limbs);
    if (compare == 0){
        return LT_SmallInteger_new(0);
    }
    if (compare > 0){
        result_limbs = subtract_abs_limbs(
            lhs.limb_count,
            lhs.limbs,
            rhs.limb_count,
            rhs.limbs,
            &result_count
        );
        return make_integer_from_limbs(lhs.negative, result_count, result_limbs);
    }

    result_limbs = subtract_abs_limbs(
        rhs.limb_count,
        rhs.limbs,
        lhs.limb_count,
        lhs.limbs,
        &result_count
    );
    return make_integer_from_limbs(rhs.negative, result_count, result_limbs);
}

LT_Value LT_Integer_subtract(LT_Value left, LT_Value right){
    LT_IntegerRef lhs;
    integer_ref_init(&lhs, left);
    LT_IntegerRef rhs;
    integer_ref_init(&rhs, right);
    size_t result_count;
    uint32_t* result_limbs;
    int compare;

    if (lhs.limb_count <= 2 && rhs.limb_count <= 2){
        int64_t a, b;
        if (limbs_fit_fixnum(lhs.negative, lhs.limb_count, lhs.limbs, &a)
            && limbs_fit_fixnum(rhs.negative, rhs.limb_count, rhs.limbs, &b)){
            int64_t diff = a - b;
            if (LT_SmallInteger_in_range(diff)){
                return LT_SmallInteger_new(diff);
            }
        }
    }

    /* Subtraction is addition with the sign of rhs flipped */
    if (lhs.negative != rhs.negative){
        result_limbs = add_abs_limbs(
            lhs.limb_count,
            lhs.limbs,
            rhs.limb_count,
            rhs.limbs,
            &result_count
        );
        return make_integer_from_limbs(lhs.negative, result_count, result_limbs);
    }

    compare = compare_abs_limbs(lhs.limb_count, lhs.limbs, rhs.limb_count, rhs.limbs);
    if (compare == 0){
        return LT_SmallInteger_new(0);
    }
    if (compare > 0){
        result_limbs = subtract_abs_limbs(
            lhs.limb_count,
            lhs.limbs,
            rhs.limb_count,
            rhs.limbs,
            &result_count
        );
        return make_integer_from_limbs(lhs.negative, result_count, result_limbs);
    }

    result_limbs = subtract_abs_limbs(
        rhs.limb_count,
        rhs.limbs,
        lhs.limb_count,
        lhs.limbs,
        &result_count
    );
    return make_integer_from_limbs(!rhs.negative, result_count, result_limbs);
}

LT_Value LT_Integer_multiply(LT_Value left, LT_Value right){
    LT_IntegerRef lhs;
    integer_ref_init(&lhs, left);
    LT_IntegerRef rhs;
    integer_ref_init(&rhs, right);
    size_t result_count;
    uint32_t* result_limbs;

    if (lhs.limb_count == 0 || rhs.limb_count == 0){
        return LT_SmallInteger_new(0);
    }

    if (lhs.limb_count == 1 && rhs.limb_count == 1){
        uint64_t product = (uint64_t)lhs.limbs[0] * (uint64_t)rhs.limbs[0];
        int negative = lhs.negative != rhs.negative;
        int64_t small_value;
        uint32_t limbs[2];
        limbs[0] = (uint32_t)product;
        limbs[1] = (uint32_t)(product >> 32);
        if (limbs_fit_fixnum(negative, 2, limbs, &small_value)){
            return LT_SmallInteger_new(small_value);
        }
        return make_integer_from_limbs(negative, limbs[1] ? 2 : 1, limbs);
    }

    result_limbs = multiply_abs_limbs(
        lhs.limb_count,
        lhs.limbs,
        rhs.limb_count,
        rhs.limbs,
        &result_count
    );
    return make_integer_from_limbs(lhs.negative != rhs.negative, result_count, result_limbs);
}

void LT_Integer_divmod(
    LT_Value dividend,
    LT_Value divisor,
    LT_Value* quotient,
    LT_Value* remainder
){
    LT_IntegerRef lhs;
    integer_ref_init(&lhs, dividend);
    LT_IntegerRef rhs;
    integer_ref_init(&rhs, divisor);
    size_t quotient_count;
    size_t remainder_count;
    uint32_t* quotient_limbs;
    uint32_t* remainder_limbs;

    if (rhs.limb_count == 0){
        LT_error("Division by zero");
    }

    divide_abs_values(
        lhs.limb_count,
        lhs.limbs,
        rhs.limb_count,
        rhs.limbs,
        &quotient_count,
        &quotient_limbs,
        &remainder_count,
        &remainder_limbs
    );

    *quotient = make_integer_from_limbs(lhs.negative != rhs.negative, quotient_count, quotient_limbs);
    *remainder = make_integer_from_limbs(lhs.negative, remainder_count, remainder_limbs);
}

LT_Value LT_Integer_gcd(LT_Value left, LT_Value right){
    LT_Value a = LT_Integer_abs(left);
    LT_Value b = LT_Integer_abs(right);

    /* Fast path: both fit in int64_t — use plain 64-bit Euclidean algorithm */
    if (LT_Value_is_fixnum(a) && LT_Value_is_fixnum(b)){
        int64_t x = LT_SmallInteger_value(a);
        int64_t y = LT_SmallInteger_value(b);

        while (y != 0){
            int64_t t = y;
            y = x % y;
            x = t;
        }
        return LT_SmallInteger_new(x);
    }

    while (!LT_Integer_is_zero(b)){
        LT_Value quotient;
        LT_Value remainder;

        LT_Integer_divmod(a, b, &quotient, &remainder);
        (void)quotient;
        a = b;
        b = LT_Integer_abs(remainder);

        /* Drop back to the fast path as soon as both fit in a fixnum */
        if (LT_Value_is_fixnum(a) && LT_Value_is_fixnum(b)){
            int64_t x = LT_SmallInteger_value(a);
            int64_t y = LT_SmallInteger_value(b);

            while (y != 0){
                int64_t t = y;
                y = x % y;
                x = t;
            }
            return LT_SmallInteger_new(x);
        }
    }

    return a;
}

bool LT_Integer_to_int64(LT_Value value, int64_t* result){
    LT_IntegerRef ref;
    integer_ref_init(&ref, value);
    uint64_t magnitude = 0;

    if (ref.limb_count > 2){
        return false;
    }
    if (ref.limb_count >= 1){
        magnitude = ref.limbs[0];
    }
    if (ref.limb_count == 2){
        magnitude |= (uint64_t)ref.limbs[1] << 32;
    }

    if (!ref.negative){
        if (magnitude > INT64_MAX){
            return false;
        }
        *result = (int64_t)magnitude;
        return true;
    }

    if (magnitude > (uint64_t)INT64_MAX + 1){
        return false;
    }
    if (magnitude == (uint64_t)INT64_MAX + 1){
        *result = INT64_MIN;
        return true;
    }

    *result = -(int64_t)magnitude;
    return true;
}

bool LT_Integer_to_uint32(LT_Value value, uint32_t* result){
    LT_IntegerRef ref;
    integer_ref_init(&ref, value);

    if (ref.negative || ref.limb_count > 1){
        return false;
    }
    if (ref.limb_count == 0){
        *result = 0;
        return true;
    }

    *result = ref.limbs[0];
    return true;
}

double LT_Integer_to_double(LT_Value value){
    LT_IntegerRef ref;
    integer_ref_init(&ref, value);
    double result = 0.0;
    size_t index;

    for (index = ref.limb_count; index > 0; index--){
        result = result * 4294967296.0 + (double)ref.limbs[index - 1];
    }

    return ref.negative ? -result : result;
}

/* Multiply a non-negative integer by a small constant (fits in uint32_t).
   Uses a single O(n) carry loop instead of the full two-dimensional
   multiply_abs_limbs. */
static LT_Value integer_multiply_by_small(LT_Value value, uint32_t factor){
    LT_IntegerRef ref;
    integer_ref_init(&ref, value);
    uint32_t* result;
    size_t result_count;
    size_t index;
    uint64_t carry = 0;

    if (ref.limb_count == 0 || factor == 0){
        return LT_SmallInteger_new(0);
    }

    result = GC_MALLOC_ATOMIC(sizeof(uint32_t) * (ref.limb_count + 1));
    for (index = 0; index < ref.limb_count; index++){
        uint64_t product = (uint64_t)ref.limbs[index] * factor + carry;
        result[index] = (uint32_t)product;
        carry = product >> 32;
    }
    result[ref.limb_count] = (uint32_t)carry;
    result_count = trim_limb_count(ref.limb_count + 1, result);
    return make_integer_from_limbs(ref.negative, result_count, result);
}

LT_Value LT_BigInteger_new_from_digits(const char* digits){
    const char* cursor = digits;
    LT_Value result = LT_SmallInteger_new(0);
    int negative = 0;

    if (*cursor == '+' || *cursor == '-'){
        negative = *cursor == '-';
        cursor++;
    }
    if (*cursor == '\0'){
        LT_error("Invalid integer literal");
    }

    while (*cursor != '\0'){
        unsigned char ch = (unsigned char)*cursor;

        if (ch < '0' || ch > '9'){
            LT_error("Invalid integer literal");
        }

        result = integer_multiply_by_small(result, 10);
        result = LT_Integer_add(result, LT_SmallInteger_new((int64_t)(ch - '0')));
        cursor++;
    }

    if (negative){
        result = LT_Integer_negate(result);
    }

    return result;
}

char* LT_BigInteger_to_decimal_cstr(LT_Value value){
    LT_IntegerRef ref;
    integer_ref_init(&ref, value);
    uint32_t* limbs;
    size_t limb_count;
    uint32_t* chunks;
    size_t chunk_count = 0;
    size_t chunk_capacity;
    size_t digit_count;
    char* buffer;
    size_t offset = 0;
    size_t index;

    if (!LT_BigInteger_p(value)){
        LT_type_error(value, &LT_BigInteger_class);
    }

    limbs = copy_limbs(ref.limb_count, ref.limbs);
    limb_count = ref.limb_count;
    chunk_capacity = limb_count == 0 ? 1 : limb_count * 2 + 1;
    chunks = GC_MALLOC_ATOMIC(sizeof(uint32_t) * chunk_capacity);

    while (limb_count > 0){
        size_t limb_index;
        uint64_t remainder = 0;

        for (limb_index = limb_count; limb_index > 0; limb_index--){
            uint64_t combined = (remainder << 32) | limbs[limb_index - 1];
            limbs[limb_index - 1] = (uint32_t)(combined / 1000000000U);
            remainder = combined % 1000000000U;
        }

        chunks[chunk_count++] = (uint32_t)remainder;
        limb_count = trim_limb_count(limb_count, limbs);
    }

    if (chunk_count == 0){
        chunks[chunk_count++] = 0;
    }

    digit_count = ref.negative ? 1 : 0;
    {
        uint32_t top_chunk = chunks[chunk_count - 1];
        do {
            digit_count++;
            top_chunk /= 10;
        } while (top_chunk != 0);
    }
    if (chunk_count > 1){
        digit_count += (chunk_count - 1) * 9;
    }

    buffer = GC_MALLOC_ATOMIC(digit_count + 1);
    if (ref.negative){
        buffer[offset++] = '-';
    }

    offset += (size_t)sprintf(buffer + offset, "%u", chunks[chunk_count - 1]);
    for (index = chunk_count - 1; index > 0; index--){
        offset += (size_t)sprintf(buffer + offset, "%09u", chunks[index - 1]);
    }
    buffer[offset] = '\0';

    return buffer;
}
