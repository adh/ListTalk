/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Closure.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/MethodDescriptor.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/Class.h>

#include <stddef.h>
#include <stdint.h>

struct LT_MethodDescriptor_s {
    LT_Object base;
    LT_Value selector;
    LT_Value callable;
    LT_Value implementing_class;
};

static LT_Slot_Descriptor MethodDescriptor_slots[] = {
    {"selector", offsetof(LT_MethodDescriptor, selector), &LT_SlotType_ReadonlyObject},
    {"callable", offsetof(LT_MethodDescriptor, callable), &LT_SlotType_ReadonlyObject},
    {
        "implementing-class",
        offsetof(LT_MethodDescriptor, implementing_class),
        &LT_SlotType_ReadonlyObject
    },
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static void MethodDescriptor_debugPrintOn(LT_Value obj, FILE* stream){
    LT_MethodDescriptor* method = LT_MethodDescriptor_from_value(obj);

    fputs("#<MethodDescriptor ", stream);
    LT_Value_debugPrintOn(method->selector, stream);
    fputs(" in ", stream);
    LT_Value_debugPrintOn(method->implementing_class, stream);
    fputc('>', stream);
}

LT_DEFINE_PRIMITIVE(
    method_descriptor_method_selector,
    "MethodDescriptor>>selector",
    "(self)",
    "Return method selector."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_MethodDescriptor_selector(LT_MethodDescriptor_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    method_descriptor_method_callable,
    "MethodDescriptor>>callable",
    "(self)",
    "Return method callable."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_MethodDescriptor_callable(LT_MethodDescriptor_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    method_descriptor_method_implementing_class,
    "MethodDescriptor>>implementing-class",
    "(self)",
    "Return class that directly implements the method."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_MethodDescriptor_implementing_class(
        LT_MethodDescriptor_from_value(self)
    );
}

static LT_Method_Descriptor MethodDescriptor_methods[] = {
    {"selector", &method_descriptor_method_selector},
    {"callable", &method_descriptor_method_callable},
    {"implementing-class", &method_descriptor_method_implementing_class},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_MethodDescriptor) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "MethodDescriptor",
    .documentation = "Immutable description of a class method.",
    .instance_size = sizeof(LT_MethodDescriptor),
    .class_flags = LT_CLASS_FLAG_FINAL | LT_CLASS_FLAG_IMMUTABLE,
    .debugPrintOn = MethodDescriptor_debugPrintOn,
    .slots = MethodDescriptor_slots,
    .methods = MethodDescriptor_methods,
};

LT_Value LT_MethodDescriptor_new(LT_Value selector,
                                LT_Value callable,
                                LT_Value implementing_class){
    LT_MethodDescriptor* method;

    if (!LT_Symbol_p(selector)){
        LT_type_error(selector, &LT_Symbol_class);
    }
    if (!LT_Primitive_p(callable) && !LT_Closure_p(callable)){
        LT_error("MethodDescriptor callable must be primitive or closure");
    }
    (void)LT_Class_from_object(implementing_class);

    method = LT_Class_ALLOC(LT_MethodDescriptor);
    method->selector = selector;
    method->callable = callable;
    method->implementing_class = implementing_class;
    return (LT_Value)(uintptr_t)method;
}

LT_Value LT_MethodDescriptor_selector(LT_MethodDescriptor* method){
    return method->selector;
}

LT_Value LT_MethodDescriptor_callable(LT_MethodDescriptor* method){
    return method->callable;
}

LT_Value LT_MethodDescriptor_implementing_class(LT_MethodDescriptor* method){
    return method->implementing_class;
}
