#include <ListTalk/Class.h>

LT_Class LT_Class_class = {0};
LT_Class LT_Class_class_class = {0};

static LT_Class** make_single_superclass_list(LT_Class* superclass){
    LT_Class** superclasses = GC_MALLOC(sizeof(LT_Class*) * 2);
    superclasses[0] = superclass;
    superclasses[1] = NULL;
    return superclasses;
}

void LT_init_native_class(LT_Class* klass){
    LT_Class_Descriptor* descriptor = klass->native_descriptor;
    LT_Class* metaclass = klass->base.klass;

    if (metaclass != NULL){
        if (metaclass->base.klass == NULL){
            metaclass->base.klass = &LT_Class_class_class;
        }
        if (metaclass->instance_size == 0){
            metaclass->instance_size = sizeof(LT_Class);
        }
        if (metaclass->superclasses == NULL){
            LT_Class* superclass = &LT_Class_class;
            if (descriptor != NULL && descriptor->metaclass_superclass != NULL){
                superclass = descriptor->metaclass_superclass;
            }
            metaclass->superclasses = make_single_superclass_list(superclass);
        }
    }

    if (klass->base.klass == NULL){
        klass->base.klass = &LT_Class_class;
    }
    if (descriptor != NULL){
        if (klass->instance_size == 0){
            klass->instance_size = descriptor->instance_size;
        }
        if (klass->class_flags == 0){
            klass->class_flags = descriptor->class_flags;
        }
        if (klass->debugPrintOn == NULL){
            klass->debugPrintOn = descriptor->debugPrintOn;
        }
    }
    if (klass->superclasses == NULL){
        LT_Class* superclass = &LT_Class_class;
        if (descriptor != NULL && descriptor->superclass != NULL){
            superclass = descriptor->superclass;
        }
        klass->superclasses = make_single_superclass_list(superclass);
    }
}
