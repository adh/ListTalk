/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/AsconRNG.h>
#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/RandomNumberGenerator.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/utils/crypto/ascon.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/random.h>

struct LT_AsconRNG_s {
    LT_Object base;
    LT_AsconXOF128 xof;
};

static int all_zero_p(const uint8_t* bytes, size_t length){
    size_t i;

    for (i = 0; i < length; i++){
        if (bytes[i] != 0){
            return 0;
        }
    }
    return 1;
}

static LT_AsconRNG* AsconRNG_new_from_seed(const uint8_t* seed, size_t length){
    LT_AsconRNG* rng;

    if (length != LT_ASCON_XOF128_SEED_LENGTH){
        LT_error("AsconRNG seed must be 32 bytes");
    }
    if (all_zero_p(seed, length)){
        LT_error("AsconRNG seed must not be all zero");
    }

    rng = LT_Class_ALLOC(LT_AsconRNG);
    LT_AsconXOF128_init(&rng->xof, seed, length);
    return rng;
}

static void AsconRNG_debugPrintOn(LT_Value obj, FILE* stream){
    fprintf(stream, "#<AsconRNG %p>", (void*)LT_AsconRNG_from_value(obj));
}

LT_DEFINE_PRIMITIVE(
    ascon_rng_class_method_from_bytevector,
    "AsconRNG class>>fromByteVector:",
    "(self seed)",
    "Return an Ascon-XOF128 random number generator initialized from 32 seed bytes."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_ByteVector* seed;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, seed, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);

    if (self != (LT_Value)(uintptr_t)&LT_AsconRNG_class){
        LT_error("fromByteVector: class method is only supported on AsconRNG");
    }
    return (LT_Value)(uintptr_t)AsconRNG_new_from_seed(
        LT_ByteVector_bytes(seed),
        LT_ByteVector_length(seed)
    );
}

LT_DEFINE_PRIMITIVE(
    ascon_rng_class_method_random,
    "AsconRNG class>>random",
    "(self)",
    "Return an Ascon-XOF128 random number generator initialized from system entropy."
){
    LT_Value cursor = arguments;
    LT_Value self;
    uint8_t seed[LT_ASCON_XOF128_SEED_LENGTH];
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    if (self != (LT_Value)(uintptr_t)&LT_AsconRNG_class){
        LT_error("random class method is only supported on AsconRNG");
    }
    do {
        if (getentropy(seed, sizeof(seed)) != 0){
            LT_system_error("Could not get entropy", errno);
        }
    } while (all_zero_p(seed, sizeof(seed)));

    return (LT_Value)(uintptr_t)AsconRNG_new_from_seed(seed, sizeof(seed));
}

LT_DEFINE_PRIMITIVE(
    ascon_rng_method_bytes,
    "AsconRNG>>bytes:",
    "(self length)",
    "Return a bytevector with length cryptographically strong random bytes."
){
    LT_Value cursor = arguments;
    LT_AsconRNG* rng;
    int64_t length_value;
    size_t length;
    LT_ByteVector* bytevector;
    uint8_t* bytes;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, rng, LT_AsconRNG*, LT_AsconRNG_from_value);
    LT_FIXNUM_ARG(cursor, length_value);
    LT_ARG_END(cursor);

    length = LT_Number_nonnegative_size_from_int64(
        length_value,
        "Negative length",
        "Length out of supported range"
    );
    bytes = GC_MALLOC_ATOMIC(length == 0 ? 1 : length);
    LT_AsconXOF128_squeeze(&rng->xof, bytes, length);
    bytevector = LT_ByteVector_new(bytes, length);
    return (LT_Value)(uintptr_t)bytevector;
}

static LT_Method_Descriptor AsconRNG_methods[] = {
    {"bytes:", &ascon_rng_method_bytes},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor AsconRNG_class_methods[] = {
    {"fromByteVector:", &ascon_rng_class_method_from_bytevector},
    {"random", &ascon_rng_class_method_random},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_AsconRNG) {
    .superclass = &LT_RandomNumberGenerator_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "AsconRNG",
    .documentation = "Cryptographically secure Ascon-XOF128 random number generator.",
    .instance_size = sizeof(LT_AsconRNG),
    .debugPrintOn = AsconRNG_debugPrintOn,
    .methods = AsconRNG_methods,
    .class_methods = AsconRNG_class_methods,
};
