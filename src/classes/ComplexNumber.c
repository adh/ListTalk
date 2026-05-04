/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/ComplexNumber.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/macros/method_macros.h>

struct LT_ComplexNumber_s {
    LT_Object base;
};

LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    complexNumber_method_real,
    "ComplexNumber>>real",
    "Return receiver's real component."
)

LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    complexNumber_method_imaginary,
    "ComplexNumber>>imaginary",
    "Return receiver's imaginary component."
)

static LT_Method_Descriptor ComplexNumber_methods[] = {
    {"real", &complexNumber_method_real},
    {"imaginary", &complexNumber_method_imaginary},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_ComplexNumber) {
    .superclass = &LT_Number_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "ComplexNumber",
    .instance_size = sizeof(LT_ComplexNumber),
    .methods = ComplexNumber_methods,
    .class_flags = LT_CLASS_FLAG_ABSTRACT
        | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
};
