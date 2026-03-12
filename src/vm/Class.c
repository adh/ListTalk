/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/Class.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/macros/decl_macros.h>

#include <inttypes.h>

static LT_Value object_slot_ref(LT_Class_Slot* slot, LT_Value object){
    LT_Value* val = LT_VALUE_POINTER_VALUE(object) + slot->offset;
    return *val;
}
static void object_slot_set(LT_Class_Slot* slot, LT_Value object, LT_Value value){
    LT_Value* val = LT_VALUE_POINTER_VALUE(object) + slot->offset;
    *val = value;
}

LT_SlotType LT_SlotType_Object = {
    .ref = object_slot_ref,
    .set = object_slot_set,
};

static void Class_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Class* klass = (LT_Class*)LT_VALUE_POINTER_VALUE(obj);

    if (klass == &LT_Class_class_class){
        fputs("#<Class Metaclass>", stream);
        return;
    }
    if (LT_Symbol_p(klass->name)){
        fputs("#<Class ", stream);
        fputs(
            LT_Symbol_name(LT_Symbol_from_value(klass->name)),
            stream
        );
        fputc('>', stream);
        return;
    }

    fprintf(stream, "#<Class 0x%" PRIxPTR ">", (uintptr_t)klass);
}

LT_DEFINE_CLASS(LT_Class) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Class",
    .instance_size = sizeof(LT_Class),
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
    .debugPrintOn = Class_debugPrintOn,
};

static LT_Class** make_single_superclass_list(LT_Class* superclass){
    LT_Class** superclasses;

    if (superclass == NULL){
        superclasses = GC_MALLOC(sizeof(LT_Class*));
        superclasses[0] = NULL;
        return superclasses;
    }

    superclasses = GC_MALLOC(sizeof(LT_Class*) * 2);
    superclasses[0] = superclass;
    superclasses[1] = NULL;
    return superclasses;
}

void LT_init_native_class(LT_Class* klass){
    LT_Class_Descriptor* descriptor = klass->native_descriptor;
    LT_Class* metaclass;

    if (descriptor == NULL){
        return;
    }

    /* Mark as in-progress before recursive initialization to break cycles. */
    klass->native_descriptor = NULL;

    if (descriptor->superclass != NULL){
        LT_init_native_class(descriptor->superclass);
    }
    if (descriptor->metaclass_superclass != NULL){
        LT_init_native_class(descriptor->metaclass_superclass);
    }

    metaclass = klass->base.klass;

    klass->instance_size = descriptor->instance_size;
    klass->class_flags = (unsigned int)descriptor->class_flags;
    klass->debugPrintOn = descriptor->debugPrintOn;
    if (descriptor->name == NULL){
        klass->name = LT_NIL;
    } else {
        klass->name = LT_Symbol_new(descriptor->name);
    }
    klass->superclasses = make_single_superclass_list(descriptor->superclass);

    if (metaclass != NULL){
        metaclass->base.klass = &LT_Class_class_class;
        if (metaclass->instance_size == 0){
            metaclass->instance_size = sizeof(LT_Class);
        }
        if (metaclass->debugPrintOn == NULL){
            metaclass->debugPrintOn = Class_debugPrintOn;
        }
        metaclass->superclasses =
            make_single_superclass_list(descriptor->metaclass_superclass);
    }
}
