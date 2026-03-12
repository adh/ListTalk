/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Object.h>
#include <ListTalk/macros/decl_macros.h>

LT_DEFINE_CLASS(LT_Object) {
    .superclass = NULL,
    .metaclass_superclass = &LT_Class_class,
    .name = "Object",
    .instance_size = sizeof(LT_Object),
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
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
