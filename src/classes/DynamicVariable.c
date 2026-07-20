/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/DynamicVariable.h>
#include <ListTalk/classes/IdentityDictionary.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/thread_state.h>

#include <inttypes.h>
#include <stddef.h>

struct LT_DynamicVariable_s {
    LT_Object base;
    LT_Value default_value;
};

static LT_IdentityDictionary* dynamic_value_dictionary(void){
    LT_ThreadState* state = LT_thread_state();

    if (state->dynamic_values == NULL){
        state->dynamic_values = LT_WeakKeyIdentityDictionary_new();
    }
    return (LT_IdentityDictionary*)state->dynamic_values;
}

static void DynamicVariable_debugPrintOn(LT_Value obj, FILE* stream){
    LT_DynamicVariable* variable = LT_DynamicVariable_from_value(obj);

    fprintf(stream, "#<DynamicVariable %p default=", (void*)variable);
    LT_Value_debugPrintOn(variable->default_value, stream);
    fputc('>', stream);
}

LT_DEFINE_PRIMITIVE(
    dynamicvariable_class_method_new,
    "DynamicVariable class>>new:",
    "(self defaultValue)",
    "Create a dynamic variable with default value."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value default_value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, default_value);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_DynamicVariable_class){
        LT_error("new: class method is only supported on DynamicVariable");
    }
    return (LT_Value)(uintptr_t)LT_DynamicVariable_new(default_value);
}

LT_DEFINE_PRIMITIVE(
    dynamicvariable_method_value,
    "DynamicVariable>>value",
    "(self)",
    "Return the current dynamic value or the default value."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_DynamicVariable_value(LT_DynamicVariable_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    dynamicvariable_method_value_put,
    "DynamicVariable>>value:",
    "(self value)",
    "Set the current thread dynamic value."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    LT_DynamicVariable_setValue(LT_DynamicVariable_from_value(self), value);
    return value;
}

static LT_Slot_Descriptor DynamicVariable_slots[] = {
    {"default-value", offsetof(LT_DynamicVariable, default_value), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static LT_Method_Descriptor DynamicVariable_methods[] = {
    {"value", &dynamicvariable_method_value},
    {"value:", &dynamicvariable_method_value_put},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor DynamicVariable_class_methods[] = {
    {"new:", &dynamicvariable_class_method_new},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_DynamicVariable) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "DynamicVariable",
    .documentation = "Dynamically scoped variable binding.",
    .instance_size = sizeof(LT_DynamicVariable),
    .debugPrintOn = DynamicVariable_debugPrintOn,
    .slots = DynamicVariable_slots,
    .methods = DynamicVariable_methods,
    .class_methods = DynamicVariable_class_methods,
};

LT_DynamicVariable* LT_DynamicVariable_new(LT_Value default_value){
    LT_DynamicVariable* variable = LT_Class_ALLOC(LT_DynamicVariable);

    variable->default_value = default_value;
    return variable;
}

LT_Value LT_DynamicVariable_value(LT_DynamicVariable* variable){
    LT_ThreadState* state = LT_thread_state();
    LT_Value value;

    if (state->dynamic_values == NULL){
        return variable->default_value;
    }
    if (!LT_IdentityDictionary_at(
        (LT_IdentityDictionary*)state->dynamic_values,
        (LT_Value)(uintptr_t)variable,
        &value
    )){
        return variable->default_value;
    }
    return value;
}

void LT_DynamicVariable_setValue(LT_DynamicVariable* variable, LT_Value value){
    LT_IdentityDictionary_atPut(
        dynamic_value_dictionary(),
        (LT_Value)(uintptr_t)variable,
        value
    );
}
