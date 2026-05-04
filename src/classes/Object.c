/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Object.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/macros/method_macros.h>
#include <ListTalk/vm/value.h>

LT_DEFINE_PRIMITIVE(
    object_method_class,
    "Object>>class",
    "(self)",
    "Return receiver class."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_Value_class(self);
}

LT_DEFINE_PRIMITIVE(
    object_method_slot,
    "Object>>slot:",
    "(self slot)",
    "Read receiver slot."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value slot_name;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, slot_name);
    LT_ARG_END(cursor);
    return LT_Object_slot_ref(self, slot_name);
}

LT_DEFINE_PRIMITIVE(
    object_method_slot_put,
    "Object>>slot:put:",
    "(self slot value)",
    "Write receiver slot."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value slot_name;
    LT_Value value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, slot_name);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Object_slot_set(self, slot_name, value);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    object_method_identity_equal,
    "Object>>==",
    "(self other)",
    "Return true when receiver and argument are identical (same object).",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    return self == other ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    object_method_equal,
    "Object>>=",
    "(self other)",
    "Return true when receiver and argument are structurally equal (equal?).",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    return LT_Value_equal_p(self, other) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    object_method_subclass_responsibility,
    "Object>>subclassResponsibility",
    "Signal that the receiver's class must implement this method."
)

static LT_Method_Descriptor Object_methods[] = {
    {"class", &object_method_class},
    {"slot:", &object_method_slot},
    {"slot:put:", &object_method_slot_put},
    {"==", &object_method_identity_equal},
    {"=",  &object_method_equal},
    {"subclassResponsibility", &object_method_subclass_responsibility},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Object) {
    .superclass = NULL,
    .metaclass_superclass = &LT_Class_class,
    .name = "Object",
    .instance_size = sizeof(LT_Object),
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
    .methods = Object_methods,
};

LT_Value LT_Object_slot_ref(LT_Value object, LT_Value slot_name){
    LT_Class* klass = LT_Value_class(object);
    LT_Class_Slot* slot = LT_Class_lookup_slot(klass, slot_name);

    if (slot == NULL){
        LT_error("Unknown slot");
    }

    return slot->type->ref(slot, object);
}

LT_Value LT_Object_slot_set(LT_Value object, LT_Value slot_name, LT_Value value){
    LT_Class* klass = LT_Value_class(object);
    LT_Class_Slot* slot = LT_Class_lookup_slot(klass, slot_name);

    if (slot == NULL){
        LT_error("Unknown slot");
    }

    slot->type->set(slot, object, value);
    return value;
}
