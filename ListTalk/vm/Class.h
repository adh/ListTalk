/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__VM_Class__
#define H__ListTalk__VM_Class__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/vm/value.h>
#include <ListTalk/vm/error.h>

LT__BEGIN_DECLS

typedef void(*LT_Class_debugPrintOn_Func)(LT_Value self, FILE* stream);
typedef size_t(*LT_Class_hash_Func)(LT_Value self);
typedef int(*LT_Class_equal_p_Func)(LT_Value self, LT_Value other);

typedef struct LT_SlotType_s LT_SlotType;
typedef struct LT_Primitive_s LT_Primitive;

typedef struct LT_Class_Slot {
    LT_Value name;
    size_t offset;
    LT_SlotType* type;
} LT_Class_Slot;

struct LT_SlotType_s {
    LT_Value (*ref)(LT_Class_Slot* slot, LT_Value object);
    void (*set)(LT_Class_Slot* slot, LT_Value object, LT_Value value);
};

extern LT_SlotType LT_SlotType_Object;
extern LT_SlotType LT_SlotType_ReadonlyObject;

#define LT_CLASS_FLAG_FLEXIBLE   1
#define LT_CLASS_FLAG_SPECIAL    2
#define LT_CLASS_FLAG_ABSTRACT   4
#define LT_CLASS_FLAG_FINAL      8
#define LT_CLASS_FLAG_IMMUTABLE 16
#define LT_CLASS_FLAG_SCALAR    32
#define LT_CLASS_FLAG_ALLOCATABLE 64

typedef struct LT_Class_Descriptor_s LT_Class_Descriptor;

#define LT_STATIC_CLASS(name) ((LT_Value)(uintptr_t)&name##_class)

struct LT_Class_s {
    LT_Object base;
    LT_Class** superclasses; /* NULL terminated */
    LT_Value precedence_list; /* ImmutableList value or untagged ImmutableList storage */
    size_t instance_size;
    unsigned int class_flags;
    size_t slot_count;
    LT_Class_Slot* slots;
    LT_Value methods;
    LT_Value method_cache;
    uintptr_t cache_version;
    LT_Value name;
    LT_Class_debugPrintOn_Func debugPrintOn;
    LT_Class_hash_Func hash;
    LT_Class_equal_p_Func equal_p;
    LT_Value documentation;
    LT_Class_Descriptor* native_descriptor; /* Native class descriptor */
};

static inline LT_Value LT_Class_precedence_list(LT_Class* klass){
    LT_Value precedence_list = klass->precedence_list;

    if (precedence_list == LT_INVALID || precedence_list == LT_NIL){
        return LT_NIL;
    }

    if (LT_VALUE_POINTER_TAG(precedence_list) == LT_VALUE_POINTER_TAG_IMMUTABLE_LIST){
        return precedence_list;
    }

    return precedence_list | LT_VALUE_POINTER_TAG_IMMUTABLE_LIST;
}

/* Inlined here in order to resolve circular dependencies */
extern LT_Class LT_Object_class;
extern LT_Class LT_Object_class_class;
extern LT_Class LT_Class_class;
extern LT_Class LT_Class_class_class;
static inline LT_Class* LT_Class_from_object(LT_Value obj){
    LT_Class* object_class = LT_Value_class(obj);

    while (object_class != NULL){
        if (object_class == &LT_Class_class){
            return (LT_Class*)LT_VALUE_POINTER_VALUE(obj);
        }
        if (object_class->superclasses == NULL){
            break;
        }
        object_class = object_class->superclasses[0];
    }

    LT_type_error(obj, &LT_Class_class);
    return NULL;
}


extern void* LT_Class_alloc(LT_Class* klass);

#define LT_Class_ALLOC(type) (type*)(LT_Class_alloc(&type##_class))

extern void* LT_Class_alloc_flexible(LT_Class* klass, size_t flex);

#define LT_Class_ALLOC_FLEXIBLE(type, flex) \
    (type*)(LT_Class_alloc_flexible(&type##_class, flex))

typedef struct LT_Slot_Descriptor {
    char* name;
    size_t offset;
    LT_SlotType* type;
} LT_Slot_Descriptor;

#define LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR {NULL, 0, NULL}
typedef struct LT_Method_Descriptor {
    char* selector;
    LT_Primitive* primitive;
} LT_Method_Descriptor;

#define LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR {NULL, NULL}
struct LT_Class_Descriptor_s {
    LT_Class* superclass;
    LT_Class* metaclass_superclass;
    char* package;
    char* name;
    size_t instance_size;
    int class_flags;
    LT_Class_debugPrintOn_Func debugPrintOn;
    LT_Class_hash_Func hash;
    LT_Class_equal_p_Func equal_p;
    LT_Slot_Descriptor* slots;
    LT_Method_Descriptor* methods;
    LT_Method_Descriptor* class_methods;
};

void LT_init_native_class(LT_Class* klass);
LT_Value LT_Class_new(LT_Value name, LT_Value superclasses, LT_Value slot_names);
LT_Value LT_Class_make_instance(LT_Class* klass);
LT_Class_Slot* LT_Class_lookup_slot(LT_Class* klass, LT_Value slot_name);
LT_Value LT_Class_slots(LT_Class* klass);
void LT_Class_addMethod(LT_Class* klass, LT_Value selector, LT_Value method);
LT_Value LT_Class_lookup_method(LT_Class* klass, LT_Value selector);
LT_Value LT_Class_lookup_method_with_next(
    LT_Class* klass,
    LT_Value selector,
    LT_Value* next_precedence_out
);

LT__END_DECLS
#endif
