/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/Class.h>

LT_Class LT_Class_class = {0};
LT_Class LT_Class_class_class = {0};

static LT_Class** make_single_superclass_list(LT_Class* superclass){
    if (superclass == NULL){
        return NULL;
    }
    LT_Class** superclasses = GC_MALLOC(sizeof(LT_Class*) * 2);
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
    klass->superclasses = make_single_superclass_list(descriptor->superclass);

    if (metaclass != NULL){
        metaclass->base.klass = &LT_Class_class_class;
        if (metaclass->instance_size == 0){
            metaclass->instance_size = sizeof(LT_Class);
        }
        metaclass->superclasses =
            make_single_superclass_list(descriptor->metaclass_superclass);
    }

    klass->native_descriptor = NULL;
}
