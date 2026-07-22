/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/RandomNumberGenerator.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/method_macros.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>

#include <stdint.h>

struct LT_RandomNumberGenerator_s {
    LT_Object base;
};

static LT_ByteVector* rng_bytes(LT_Value self, size_t length){
    LT_Value bytes = LT_SEND(self, "bytes:", LT_Number_smallinteger_from_size(
        length,
        "ByteVector length does not fit fixnum"
    ));

    return LT_ByteVector_from_value(bytes);
}

static uint64_t rng_uint64(LT_Value self){
    LT_ByteVector* bytes = rng_bytes(self, 8);
    uint64_t value = 0;
    size_t i;

    if (LT_ByteVector_length(bytes) != 8){
        LT_error("RandomNumberGenerator>>bytes: returned wrong length");
    }
    for (i = 0; i < 8; i++){
        value = (value << 8) | (uint64_t)LT_ByteVector_at(bytes, i);
    }
    return value;
}

static uint64_t rng_uint64_below(LT_Value self, uint64_t bound){
    uint64_t limit = UINT64_MAX - (UINT64_MAX % bound);

    while (1){
        uint64_t value = rng_uint64(self);

        if (value < limit){
            return value % bound;
        }
    }
}

LT_DEFINE_PRIMITIVE(
    rng_method_integer_below,
    "RandomNumberGenerator>>integerBelow:",
    "(self bound)",
    "Return a random integer greater than or equal to zero and less than bound."
){
    LT_Value cursor = arguments;
    LT_Value self;
    int64_t bound_value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_FIXNUM_ARG(cursor, bound_value);
    LT_ARG_END(cursor);

    if (bound_value <= 0){
        LT_error("Random bound must be positive");
    }
    return LT_SmallInteger_new((int64_t)rng_uint64_below(self, (uint64_t)bound_value));
}

LT_DEFINE_PRIMITIVE(
    rng_method_float,
    "RandomNumberGenerator>>float",
    "(self)",
    "Return a random float greater than or equal to zero and less than one."
){
    LT_Value cursor = arguments;
    LT_Value self;
    uint64_t value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    value = rng_uint64(self) >> 11;
    return LT_Float_new((double)value / 9007199254740992.0);
}

LT_DEFINE_PRIMITIVE(
    rng_method_integer_from_to,
    "RandomNumberGenerator>>integerFrom:to:",
    "(self from to)",
    "Return a random integer in the inclusive range from to to."
){
    LT_Value cursor = arguments;
    LT_Value self;
    int64_t from_value;
    int64_t to_value;
    uint64_t span;
    uint64_t offset;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_FIXNUM_ARG(cursor, from_value);
    LT_FIXNUM_ARG(cursor, to_value);
    LT_ARG_END(cursor);

    if (from_value > to_value){
        LT_error("Random range is empty");
    }

    span = ((uint64_t)to_value - (uint64_t)from_value) + 1;
    offset = rng_uint64_below(self, span);
    return LT_SmallInteger_new(from_value + (int64_t)offset);
}

LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_1(
    rng_method_bytes,
    "RandomNumberGenerator>>bytes:",
    length,
    "Return a bytevector with length random bytes."
)

static LT_Method_Descriptor RandomNumberGenerator_methods[] = {
    {"integerBelow:", &rng_method_integer_below},
    {"float", &rng_method_float},
    {"integerFrom:to:", &rng_method_integer_from_to},
    {"bytes:", &rng_method_bytes},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_RandomNumberGenerator) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "RandomNumberGenerator",
    .documentation = "Abstract random number generator.",
    .instance_size = sizeof(LT_RandomNumberGenerator),
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
    .methods = RandomNumberGenerator_methods,
};
