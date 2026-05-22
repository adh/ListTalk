/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/WeakReference.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/weak.h>

struct LT_WeakReference_s {
    LT_Object base;
    LT_WeakValue value;
};

LT_DEFINE_PRIMITIVE(
    weakreference_class_method_new,
    "WeakReference class>>new:",
    "(self value)",
    "Create a weak reference to value."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_WeakReference_class){
        LT_error("new: class method is only supported on WeakReference");
    }
    return (LT_Value)(uintptr_t)LT_WeakReference_new(value);
}

LT_DEFINE_PRIMITIVE(
    weakreference_method_value,
    "WeakReference>>value",
    "(self)",
    "Return referenced value or signal an error if it is no longer alive."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_WeakReference_value(LT_WeakReference_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    weakreference_method_alive_p,
    "WeakReference>>alive?",
    "(self)",
    "Return true when referenced value is still alive."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_WeakReference_alive_p(LT_WeakReference_from_value(self))
        ? LT_TRUE
        : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    weakreference_method_value_put,
    "WeakReference>>value:",
    "(self value)",
    "Set referenced value."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    LT_WeakReference_setValue(LT_WeakReference_from_value(self), value);
    return value;
}

static LT_Method_Descriptor WeakReference_methods[] = {
    {"value", &weakreference_method_value},
    {"alive?", &weakreference_method_alive_p},
    {"value:", &weakreference_method_value_put},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor WeakReference_class_methods[] = {
    {"new:", &weakreference_class_method_new},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_WeakReference) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "WeakReference",
    .documentation = "Reference that does not keep its target alive.",
    .instance_size = sizeof(LT_WeakReference),
    .methods = WeakReference_methods,
    .class_methods = WeakReference_class_methods,
};

LT_WeakReference* LT_WeakReference_new(LT_Value value){
    LT_WeakReference* reference = LT_Class_ALLOC(LT_WeakReference);

    LT_weak_box(&reference->value, value);
    return reference;
}

int LT_WeakReference_alive_p(LT_WeakReference* reference){
    return LT_weak_is_alive(reference->value);
}

LT_Value LT_WeakReference_value(LT_WeakReference* reference){
    if (!LT_WeakReference_alive_p(reference)){
        LT_error("WeakReference is not alive");
    }
    return LT_weak_unbox(reference->value);
}

void LT_WeakReference_setValue(LT_WeakReference* reference, LT_Value value){
    LT_weak_box(&reference->value, value);
}
