/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Future.h>
#include <ListTalk/classes/Object.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/method_macros.h>
#include <ListTalk/classes/Class.h>

struct LT_Future_s {
    LT_Object base;
};

LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    future_method_force,
    "Future>>force!",
    "Resolve the future if needed and return its value."
)

LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    future_method_value,
    "Future>>value",
    "Return the future's value."
)

LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    future_method_hasValue,
    "Future>>hasValue?",
    "Return true when the future has a value."
)

LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    future_method_resolved,
    "Future>>resolved?",
    "Return true when the future is resolved."
)

static LT_Method_Descriptor Future_methods[] = {
    {"force!", &future_method_force},
    {"value", &future_method_value},
    {"hasValue?", &future_method_hasValue},
    {"resolved?", &future_method_resolved},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Future) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Future",
    .documentation = "Abstract root for values that may be resolved later.",
    .instance_size = sizeof(LT_Future),
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
    .methods = Future_methods,
};
