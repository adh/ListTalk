/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Macro.h>
#include <ListTalk/classes/Closure.h>
#include <ListTalk/classes/CompoundForm.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/Class.h>

#include <stddef.h>

struct LT_Macro_s {
    LT_Value callable;
};

static LT_Slot_Descriptor Macro_slots[] = {
    {"callable", offsetof(LT_Macro, callable), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static void Macro_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Macro* macro = LT_Macro_from_value(obj);

    fputs("#<Macro ", stream);
    LT_Value_debugPrintOn(macro->callable, stream);
    fputc('>', stream);
}

LT_DEFINE_PRIMITIVE(
    macro_method_name,
    "Macro>>name",
    "(self)",
    "Return the wrapped callable's name."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    char* name;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    callable = LT_Macro_callable(LT_Macro_from_value(self));
    if (LT_Closure_p(callable)){
        return LT_Closure_name(LT_Closure_from_value(callable));
    }
    name = LT_Primitive_name(LT_Primitive_from_value(callable));
    if (name == NULL){
        return LT_NIL;
    }
    return (LT_Value)(uintptr_t)LT_String_new_cstr(name);
}

static LT_Method_Descriptor Macro_methods[] = {
    {"name", &macro_method_name},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Macro) {
    .superclass = &LT_CompoundForm_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Macro",
    .documentation = "Source transformation invoked during evaluation.",
    .instance_size = sizeof(LT_Macro),
    .class_flags = LT_CLASS_FLAG_SPECIAL,
    .debugPrintOn = Macro_debugPrintOn,
    .slots = Macro_slots,
    .methods = Macro_methods,
};

LT_Value LT_Macro_new(LT_Value callable){
    LT_Macro* macro = GC_NEW(LT_Macro);
    macro->callable = callable;
    return ((LT_Value)(uintptr_t)macro) | LT_VALUE_POINTER_TAG_MACRO;
}

LT_Value LT_Macro_callable(LT_Macro* macro){
    return macro->callable;
}
