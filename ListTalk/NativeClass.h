#ifndef H__ListTalk__Class__
#define H__ListTalk__Class__

#include <ListTalk/OOP.h>

#include <stdlib.h>
#include <stdbool.h>

typedef struct LT_NativeClass {
    LT_Class base;
} LT_NativeClass;

typedef struct LT_NativeSlotType LT_NativeSlotType;

typedef struct LT_NativeSlot_Descriptor {
    char* package;
    char* name;
    size_t offset;
    LT_NativeSlotType* type;
} LT_NativeSlot_Descriptor;

typedef struct LT_NativeClass_Descriptor {
    LT_Class* superclass;
    char* package;
    char* name;
    size_t instance_size;
    int class_flags;
    LT_NativeSlot_Descriptor* slots;
} LT_NativeClass_Descriptor;

extern LT_Class* LT_init_native_class(LT_NativeClass* klass, LT_NativeClass_Descriptor* desc);

#endif