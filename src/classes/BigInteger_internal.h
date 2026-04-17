/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__BigInteger_internal__
#define H__ListTalk__BigInteger_internal__

#include <ListTalk/classes/Integer.h>

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

static inline size_t hash_uint64(uint64_t value){
    value ^= value >> 33;
    value *= UINT64_C(0xff51afd7ed558ccd);
    value ^= value >> 33;
    value *= UINT64_C(0xc4ceb9fe1a85ec53);
    value ^= value >> 33;
    return (size_t)value;
}
bool LT_Integer_is_zero(LT_Value value);
bool LT_Integer_negative_p(LT_Value value);
int LT_Integer_compare(LT_Value left, LT_Value right);
size_t LT_Integer_hash(LT_Value value);
LT_Value LT_Integer_abs(LT_Value value);
LT_Value LT_Integer_negate(LT_Value value);
LT_Value LT_Integer_add(LT_Value left, LT_Value right);
LT_Value LT_Integer_subtract(LT_Value left, LT_Value right);
LT_Value LT_Integer_multiply(LT_Value left, LT_Value right);
void LT_Integer_divmod(
    LT_Value dividend,
    LT_Value divisor,
    LT_Value* quotient,
    LT_Value* remainder
);
LT_Value LT_Integer_gcd(LT_Value left, LT_Value right);
bool LT_Integer_to_int64(LT_Value value, int64_t* result);
bool LT_Integer_to_uint32(LT_Value value, uint32_t* result);
double LT_Integer_to_double(LT_Value value);

#endif
