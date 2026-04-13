/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/RealNumber.h>
#include <ListTalk/classes/ComplexNumber.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/decl_macros.h>

struct LT_RealNumber_s {
    LT_Object base;
};

LT_DEFINE_PRIMITIVE_FLAGS(
    realNumber_method_asFloat,
    "RealNumber>>asFloat",
    "(self)",
    "Return receiver converted to a float.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Float_new(LT_Number_to_double(self));
}

static LT_Method_Descriptor RealNumber_methods[] = {
    {"asFloat", &realNumber_method_asFloat},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_RealNumber) {
    .superclass = &LT_ComplexNumber_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "RealNumber",
    .instance_size = sizeof(LT_RealNumber),
    .methods = RealNumber_methods,
    .class_flags = LT_CLASS_FLAG_ABSTRACT
        | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
};
