/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/ReflectedBinding.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/Class.h>

#include <stddef.h>
#include <stdint.h>

struct LT_ReflectedBinding_s {
    LT_Object base;
    LT_Value symbol;
    LT_Value value;
    LT_Value flags;
};

static LT_Slot_Descriptor ReflectedBinding_slots[] = {
    {"symbol", offsetof(LT_ReflectedBinding, symbol), &LT_SlotType_ReadonlyObject},
    {"value", offsetof(LT_ReflectedBinding, value), &LT_SlotType_ReadonlyObject},
    {"flags", offsetof(LT_ReflectedBinding, flags), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static void ReflectedBinding_debugPrintOn(LT_Value obj, FILE* stream){
    LT_ReflectedBinding* binding = LT_ReflectedBinding_from_value(obj);

    fputs("#<ReflectedBinding ", stream);
    LT_Value_debugPrintOn(binding->symbol, stream);
    fputc('>', stream);
}

LT_DEFINE_PRIMITIVE(
    reflected_binding_method_symbol,
    "ReflectedBinding>>symbol",
    "(self)",
    "Return binding symbol."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_ReflectedBinding_symbol(LT_ReflectedBinding_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    reflected_binding_method_value,
    "ReflectedBinding>>value",
    "(self)",
    "Return binding value."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_ReflectedBinding_value(LT_ReflectedBinding_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    reflected_binding_method_flags,
    "ReflectedBinding>>flags",
    "(self)",
    "Return binding flags."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_SmallInteger_new(
        LT_ReflectedBinding_flags(LT_ReflectedBinding_from_value(self))
    );
}

static LT_Method_Descriptor ReflectedBinding_methods[] = {
    {"symbol", &reflected_binding_method_symbol},
    {"value", &reflected_binding_method_value},
    {"flags", &reflected_binding_method_flags},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_ReflectedBinding) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "ReflectedBinding",
    .documentation = "Immutable description of an environment binding.",
    .instance_size = sizeof(LT_ReflectedBinding),
    .class_flags = LT_CLASS_FLAG_FINAL | LT_CLASS_FLAG_IMMUTABLE,
    .debugPrintOn = ReflectedBinding_debugPrintOn,
    .slots = ReflectedBinding_slots,
    .methods = ReflectedBinding_methods,
};

LT_Value LT_ReflectedBinding_new(LT_Value symbol,
                                 LT_Value value,
                                 unsigned int flags){
    LT_ReflectedBinding* binding;

    if (!LT_Symbol_p(symbol)){
        LT_type_error(symbol, &LT_Symbol_class);
    }
    binding = LT_Class_ALLOC(LT_ReflectedBinding);
    binding->symbol = symbol;
    binding->value = value;
    binding->flags = LT_SmallInteger_new(flags);
    return (LT_Value)(uintptr_t)binding;
}

LT_Value LT_ReflectedBinding_symbol(LT_ReflectedBinding* binding){
    return binding->symbol;
}

LT_Value LT_ReflectedBinding_value(LT_ReflectedBinding* binding){
    return binding->value;
}

unsigned int LT_ReflectedBinding_flags(LT_ReflectedBinding* binding){
    return (unsigned int)LT_SmallInteger_value(binding->flags);
}
