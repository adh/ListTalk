#ifndef H__ListTalk__Class__
#define H__ListTalk__Class__

#include <ListTalk/OOP.h>

#include <ListTalk/Printer.h>

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

struct LT_Class_CoreMethods {
    uint32_t (*hash)(LT_Object* object);
    int (*equal)(LT_Object* object);
    void (*print)(LT_Object* objcect, LT_Printer* printer);
};

typedef struct LT_NativeClass_DescriptorLink {
    LT_NativeClass_Descriptor* descriptor;
    LT_Class* klass;
    LT_Class* klass_class;
} LT_NativeClass_DescriptorLink;

extern LT_Class* LT_init_native_class(LT_NativeClass* klass, LT_NativeClass_Descriptor* desc);

#endif