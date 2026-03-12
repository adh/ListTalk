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

typedef struct LT_SlotType_s LT_SlotType;

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

#define LT_CLASS_FLAG_FLEXIBLE   1
#define LT_CLASS_FLAG_SPECIAL    2
#define LT_CLASS_FLAG_ABSTRACT   4
#define LT_CLASS_FLAG_FINAL      8
#define LT_CLASS_FLAG_IMMUTABLE 16
#define LT_CLASS_FLAG_SCALAR    32

typedef struct LT_Class_Descriptor_s LT_Class_Descriptor;

struct LT_Class_s {
    LT_Object base;
    LT_Class** superclasses; /* NULL terminated */
    size_t instance_size;
    unsigned int class_flags;
    size_t slot_count;
    LT_Class_Slot* slots;
    LT_Value methods;
    LT_Value method_cache;
    uintptr_t cache_version;
    LT_Value name;
    LT_Class_debugPrintOn_Func debugPrintOn;
    LT_Value documentation;
    LT_Class_Descriptor* native_descriptor; /* Native class descriptor */
};

/* Inlined here in order to resolve circular dependencies */
extern LT_Class LT_Object_class;
extern LT_Class LT_Object_class_class;
extern LT_Class LT_Class_class;
extern LT_Class LT_Class_class_class;
static inline LT_Class* LT_Class_from_object(LT_Value obj){
    if (LT_Value_class(obj) != &LT_Class_class){
        LT_type_error(obj, &LT_Class_class);
    }
    return (LT_Class*)LT_VALUE_POINTER_VALUE(obj);
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
    LT_Value function; /* function, no special case for native code */
} LT_Method_Descriptor;

#define LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR {NULL, LT_NIL}
struct LT_Class_Descriptor_s {
    LT_Class* superclass;
    LT_Class* metaclass_superclass;
    char* package;
    char* name;
    size_t instance_size;
    int class_flags;
    LT_Class_debugPrintOn_Func debugPrintOn;
    LT_Slot_Descriptor* slots;
    LT_Method_Descriptor* methods;
    LT_Method_Descriptor* class_methods;
};

void LT_init_native_class(LT_Class* klass);

LT__END_DECLS
#endif
