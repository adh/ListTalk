/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/RandomNumberGenerator.h>
#include <ListTalk/classes/XoshiroRNG.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/random.h>

struct LT_XoshiroRNG_s {
    LT_Object base;
    uint64_t state[4];
};

static uint64_t rotl(uint64_t value, int shift){
    return (value << shift) | (value >> (64 - shift));
}

static uint64_t splitmix64_next(uint64_t* state){
    uint64_t result;

    *state += UINT64_C(0x9e3779b97f4a7c15);
    result = *state;
    result = (result ^ (result >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    result = (result ^ (result >> 27)) * UINT64_C(0x94d049bb133111eb);
    return result ^ (result >> 31);
}

static uint64_t read_u64_be(const uint8_t* bytes){
    uint64_t value = 0;
    size_t i;

    for (i = 0; i < 8; i++){
        value = (value << 8) | (uint64_t)bytes[i];
    }
    return value;
}

static void write_u64_be(uint8_t* bytes, uint64_t value){
    size_t i;

    for (i = 0; i < 8; i++){
        bytes[7 - i] = (uint8_t)(value & UINT64_C(0xff));
        value >>= 8;
    }
}

static int state_is_zero(const uint64_t state[4]){
    return state[0] == 0 && state[1] == 0 && state[2] == 0 && state[3] == 0;
}

static LT_XoshiroRNG* XoshiroRNG_new_from_state(const uint64_t state[4]){
    LT_XoshiroRNG* rng;
    size_t i;

    if (state_is_zero(state)){
        LT_error("XoshiroRNG state must not be all zero");
    }

    rng = LT_Class_ALLOC(LT_XoshiroRNG);
    for (i = 0; i < 4; i++){
        rng->state[i] = state[i];
    }
    return rng;
}

static uint64_t XoshiroRNG_next_u64(LT_XoshiroRNG* rng){
    uint64_t result = rotl(rng->state[1] * UINT64_C(5), 7) * UINT64_C(9);
    uint64_t t = rng->state[1] << 17;

    rng->state[2] ^= rng->state[0];
    rng->state[3] ^= rng->state[1];
    rng->state[1] ^= rng->state[2];
    rng->state[0] ^= rng->state[3];

    rng->state[2] ^= t;
    rng->state[3] = rotl(rng->state[3], 45);

    return result;
}

static void XoshiroRNG_debugPrintOn(LT_Value obj, FILE* stream){
    fprintf(stream, "#<XoshiroRNG %p>", (void*)LT_XoshiroRNG_from_value(obj));
}

LT_DEFINE_PRIMITIVE(
    xoshiro_rng_class_method_seed,
    "XoshiroRNG class>>seed:",
    "(self seed)",
    "Return a xoshiro256** random number generator initialized from seed."
){
    LT_Value cursor = arguments;
    LT_Value self;
    int64_t seed_value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_FIXNUM_ARG(cursor, seed_value);
    LT_ARG_END(cursor);

    if (self != (LT_Value)(uintptr_t)&LT_XoshiroRNG_class){
        LT_error("seed: class method is only supported on XoshiroRNG");
    }
    return (LT_Value)(uintptr_t)LT_XoshiroRNG_from_seed(seed_value);
}

LT_DEFINE_PRIMITIVE(
    xoshiro_rng_class_method_from_bytevector,
    "XoshiroRNG class>>fromByteVector:",
    "(self state)",
    "Return a xoshiro256** random number generator initialized from 32 state bytes."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_ByteVector* bytevector;
    uint64_t state[4];
    size_t i;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, bytevector, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);

    if (self != (LT_Value)(uintptr_t)&LT_XoshiroRNG_class){
        LT_error("fromByteVector: class method is only supported on XoshiroRNG");
    }
    if (LT_ByteVector_length(bytevector) != 32){
        LT_error("XoshiroRNG state must be 32 bytes");
    }

    for (i = 0; i < 4; i++){
        state[i] = read_u64_be(LT_ByteVector_bytes(bytevector) + (i * 8));
    }
    return (LT_Value)(uintptr_t)XoshiroRNG_new_from_state(state);
}

LT_DEFINE_PRIMITIVE(
    xoshiro_rng_class_method_random,
    "XoshiroRNG class>>random",
    "(self)",
    "Return a xoshiro256** random number generator initialized from system entropy."
){
    LT_Value cursor = arguments;
    LT_Value self;
    uint8_t bytes[32];
    uint64_t state[4];
    size_t i;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    if (self != (LT_Value)(uintptr_t)&LT_XoshiroRNG_class){
        LT_error("random class method is only supported on XoshiroRNG");
    }
    if (getentropy(bytes, sizeof(bytes)) != 0){
        LT_system_error("Could not get entropy", errno);
    }
    for (i = 0; i < 4; i++){
        state[i] = read_u64_be(bytes + (i * 8));
    }
    return (LT_Value)(uintptr_t)XoshiroRNG_new_from_state(state);
}

LT_DEFINE_PRIMITIVE(
    xoshiro_rng_method_bytes,
    "XoshiroRNG>>bytes:",
    "(self length)",
    "Return a bytevector with length random bytes."
){
    LT_Value cursor = arguments;
    LT_XoshiroRNG* rng;
    int64_t length_value;
    size_t length;
    LT_ByteVector* bytevector;
    size_t offset = 0;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, rng, LT_XoshiroRNG*, LT_XoshiroRNG_from_value);
    LT_FIXNUM_ARG(cursor, length_value);
    LT_ARG_END(cursor);

    length = LT_Number_nonnegative_size_from_int64(
        length_value,
        "Negative length",
        "Length out of supported range"
    );
    bytevector = LT_ByteVector_new_filled(length, 0);

    while (offset < length){
        uint8_t bytes[8];
        size_t chunk = length - offset;
        size_t i;

        if (chunk > sizeof(bytes)){
            chunk = sizeof(bytes);
        }
        write_u64_be(bytes, XoshiroRNG_next_u64(rng));
        for (i = 0; i < chunk; i++){
            LT_ByteVector_atPut(bytevector, offset + i, bytes[i]);
        }
        offset += chunk;
    }
    return (LT_Value)(uintptr_t)bytevector;
}

static LT_Method_Descriptor XoshiroRNG_methods[] = {
    {"bytes:", &xoshiro_rng_method_bytes},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor XoshiroRNG_class_methods[] = {
    {"seed:", &xoshiro_rng_class_method_seed},
    {"fromByteVector:", &xoshiro_rng_class_method_from_bytevector},
    {"random", &xoshiro_rng_class_method_random},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_XoshiroRNG) {
    .superclass = &LT_RandomNumberGenerator_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "XoshiroRNG",
    .documentation = "xoshiro256** random number generator.",
    .instance_size = sizeof(LT_XoshiroRNG),
    .debugPrintOn = XoshiroRNG_debugPrintOn,
    .methods = XoshiroRNG_methods,
    .class_methods = XoshiroRNG_class_methods,
};

LT_XoshiroRNG* LT_XoshiroRNG_from_seed(int64_t seed){
    uint64_t state[4];
    uint64_t splitmix_state = (uint64_t)seed;
    size_t i;

    for (i = 0; i < 4; i++){
        state[i] = splitmix64_next(&splitmix_state);
    }
    return XoshiroRNG_new_from_state(state);
}
