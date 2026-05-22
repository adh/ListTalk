/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/BindingDescriptor.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/Environment.h>

#include <stddef.h>
#include <stdint.h>

struct LT_BindingDescriptor_s {
    LT_Object base;
    LT_Value symbol;
    LT_Value value;
    LT_Value flags;
};

static LT_Slot_Descriptor BindingDescriptor_slots[] = {
    {"symbol", offsetof(LT_BindingDescriptor, symbol), &LT_SlotType_ReadonlyObject},
    {"value", offsetof(LT_BindingDescriptor, value), &LT_SlotType_ReadonlyObject},
    {"flags", offsetof(LT_BindingDescriptor, flags), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static void BindingDescriptor_debugPrintOn(LT_Value obj, FILE* stream){
    LT_BindingDescriptor* binding = LT_BindingDescriptor_from_value(obj);

    fputs("#<BindingDescriptor ", stream);
    LT_Value_debugPrintOn(binding->symbol, stream);
    fputc('>', stream);
}

LT_DEFINE_PRIMITIVE(
    binding_descriptor_method_symbol,
    "BindingDescriptor>>symbol",
    "(self)",
    "Return binding symbol."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_BindingDescriptor_symbol(LT_BindingDescriptor_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    binding_descriptor_method_value,
    "BindingDescriptor>>value",
    "(self)",
    "Return binding value."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_BindingDescriptor_value(LT_BindingDescriptor_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    binding_descriptor_method_flags,
    "BindingDescriptor>>flags",
    "(self)",
    "Return binding flags."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_SmallInteger_new(
        LT_BindingDescriptor_flags(LT_BindingDescriptor_from_value(self))
    );
}

LT_DEFINE_PRIMITIVE(
    binding_descriptor_method_constant,
    "BindingDescriptor>>constant?",
    "(self)",
    "Return true when binding is constant."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return (LT_BindingDescriptor_flags(LT_BindingDescriptor_from_value(self))
        & LT_ENV_BINDING_FLAG_CONSTANT) != 0 ? LT_TRUE : LT_FALSE;
}

static LT_Method_Descriptor BindingDescriptor_methods[] = {
    {"symbol", &binding_descriptor_method_symbol},
    {"value", &binding_descriptor_method_value},
    {"flags", &binding_descriptor_method_flags},
    {"constant?", &binding_descriptor_method_constant},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_BindingDescriptor) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "BindingDescriptor",
    .documentation = "Immutable description of an environment binding.",
    .instance_size = sizeof(LT_BindingDescriptor),
    .class_flags = LT_CLASS_FLAG_FINAL | LT_CLASS_FLAG_IMMUTABLE,
    .debugPrintOn = BindingDescriptor_debugPrintOn,
    .slots = BindingDescriptor_slots,
    .methods = BindingDescriptor_methods,
};

LT_Value LT_BindingDescriptor_new(LT_Value symbol,
                                 LT_Value value,
                                 unsigned int flags){
    LT_BindingDescriptor* binding;

    if (!LT_Symbol_p(symbol)){
        LT_type_error(symbol, &LT_Symbol_class);
    }
    binding = LT_Class_ALLOC(LT_BindingDescriptor);
    binding->symbol = symbol;
    binding->value = value;
    binding->flags = LT_SmallInteger_new(flags);
    return (LT_Value)(uintptr_t)binding;
}

LT_Value LT_BindingDescriptor_symbol(LT_BindingDescriptor* binding){
    return binding->symbol;
}

LT_Value LT_BindingDescriptor_value(LT_BindingDescriptor* binding){
    return binding->value;
}

unsigned int LT_BindingDescriptor_flags(LT_BindingDescriptor* binding){
    return (unsigned int)LT_SmallInteger_value(binding->flags);
}
