/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/RealNumber.h>
#include <ListTalk/classes/ComplexNumber.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/error.h>

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

LT_DEFINE_PRIMITIVE(
    realNumber_method_from_to_with_step_do,
    "RealNumber>>from:to:withStep:do:",
    "(self from to step callable)",
    "Call callable with each value in the half-open range from to to stepping by step."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value from;
    LT_Value to;
    LT_Value step;
    LT_Value callable;
    LT_Value current;
    int step_comparison;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, from);
    LT_OBJECT_ARG(cursor, to);
    LT_OBJECT_ARG(cursor, step);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    (void)self;

    step_comparison = LT_Number_compare(step, LT_SmallInteger_new(0));
    if (step_comparison == 0){
        LT_error("Step must not be zero");
    }

    current = from;
    if (step_comparison > 0){
        while (LT_Number_compare(current, to) < 0){
            (void)LT_APPLY(callable, current);
            current = LT_Number_add2(current, step);
        }
    } else {
        while (LT_Number_compare(current, to) > 0){
            (void)LT_APPLY(callable, current);
            current = LT_Number_add2(current, step);
        }
    }
    return LT_NIL;
}

static LT_Method_Descriptor RealNumber_methods[] = {
    {"asFloat", &realNumber_method_asFloat},
    {"from:to:withStep:do:", &realNumber_method_from_to_with_step_do},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_RealNumber) {
    .superclass = &LT_ComplexNumber_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "RealNumber",
    .documentation = "Abstract root for real numeric values.",
    .instance_size = sizeof(LT_RealNumber),
    .methods = RealNumber_methods,
    .class_flags = LT_CLASS_FLAG_ABSTRACT
        | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
};
