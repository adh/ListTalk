#ifndef H__ListTalk__OOP__
#define H__ListTalk__OOP__

#include <ListTalk/env_macros.h>
#include <ListTalk/decl_macros.h>

#include <stdlib.h>
#include <stdio.h>


LT__BEGIN_DECLS

typedef uintptr_t LT_Object;

#define LT_OBJECT_IS_IMMEDIATE(obj) (((uintptr_t)(obj) >> 63) != 0)
#define LT_OBJECT_IS_POINTER(obj) (((uintptr_t)(obj) >> 63) == 0)
#define LT_OBJECT_IMMEDIATE_TAG(obj) (((uintptr_t)(obj) >> 56) & 0xff)
#define LT_OBJECT_IMMEDIATE_VALUE(obj) ((uintptr_t)(obj) & 0x00ffffffffffffffULL)
#define LT_OBJECT_POINTER_TAG(obj) ((uintptr_t)(obj) & 0x7)
#define LT_OBJECT_POINTER_VALUE(obj) ((void*)((uintptr_t)(obj) & ~0x7))
#define LT_OBJECT_HEADER_FROM_POINTER(obj) \
    ((LT_ObjectHeader*)((uintptr_t)(obj) & ~0x7))

#define LT_OBJECT_POINTER_TAG_OBJECT 0x0
#define LT_OBJECT_POINTER_TAG_IMMUTABKE_LIST 0x1
#define LT_OBJECT_POINTER_TAG_PPAIR 0x2
#define LT_OBJECT_POINTER_TAG_CLOSURE 0x4
#define LT_OBJECT_POINTER_TAG_PRIMITIVE 0x5
#define LT_OBJECT_POINTER_TAG_MACRO 0x6
#define LT_OBJECT_POINTER_TAG_SPECIAL_FORM 0x7

#define LT_OBJECT_IMMEDIATE_TAG_NIL 0x80
#define LT_OBJECT_IMMEDIATE_TAG_SYMBOL 0x81
#define LT_OBJECT_IMMEDIATE_TAG_BOOLEAN 0x82
#define LT_OBJECT_IMMEDIATE_TAG_FIXNUM 0x83
#define LT_OBJECT_IMMEDIATE_TAG_CHARACTER 0x84
#define LT_OBJECT_IMMEDIATE_TAG_UINT8 0x85
#define LT_OBJECT_IMMEDIATE_TAG_UINT16 0x86
#define LT_OBJECT_IMMEDIATE_TAG_UINT32 0x87
#define LT_OBJECT_IMMEDIATE_TAG_FLOAT 0x88
#define LT_OBJECT_IMMEDIATE_TAG_INT8 0x89
#define LT_OBJECT_IMMEDIATE_TAG_INT16 0x8a
#define LT_OBJECT_IMMEDIATE_TAG_INT32 0x8b
#define LT_OBJECT_IMMEDIATE_TAG_SMALL_FRACTION 0x8c
#define LT_OBJECT_IMMEDIATE_TAG_IS_SHORT_STRING(tag) ((tag) & 0xf8 == 0x90)
#define LT_OBJECT_IMMEDIATE_TAG_SHORT_STRING_LENGTH(tag) ((tag) & 0x7)
#define LT_OBJECT_IMMEDIATE_TAG_INVALID 0xbf
#define LT_OBJECT_IMMEDIATE_TAG_IS_FLONUM(tag) ((tag) & 0xc0 == 0xc0)

#define LT_OBJECT_MAKE_IMMEDIATE(tag, value) \
    ((LT_Object)((((uintptr_t)(tag) & 0xff) << 56) | ((uintptr_t)(value) & 0x00ffffffffffffffULL) | 0x8000000000000000ULL))


typedef struct LT_ObjectHeader_s LT_ObjectHeader;

#define LT_CLASS_FLAG_FLEXIBLE   1
#define LT_CLASS_FLAG_SPECIAL    2
#define LT_CLASS_FLAG_ABSTRACT   4
#define LT_CLASS_FLAG_FINAL      8
#define LT_CLASS_FLAG_IMMUTABLE 16
#define LT_CLASS_FLAG_SCALAR    32

typedef struct LT_Class LT_Class;
struct LT_ObjectHeader_s{
    LT_Class* klass;
};

extern LT_Class* const LT__Immediate_classes[];
extern LT_Class* const LT__Pointer_classes[];
extern LT_Class LT_FloNum_class;

static inline LT_Class* LT_Object_class(LT_Object object){
    if (LT_OBJECT_IS_IMMEDIATE(object)){
        if (LT_OBJECT_IMMEDIATE_TAG_IS_FLONUM(LT_OBJECT_IMMEDIATE_TAG(object))){
            return &LT_FloNum_class;
        }
        return LT__Immediate_classes[LT_OBJECT_IMMEDIATE_TAG(object)];
    }
    if (LT_OBJECT_POINTER_TAG(object) != LT_OBJECT_POINTER_TAG_OBJECT){
        return LT__Pointer_classes[LT_OBJECT_POINTER_TAG(object)];
    }
    return LT_OBJECT_HEADER_FROM_POINTER(object)->klass;
}

typedef struct LT_Class_CoreMethods LT_Class_CoreMethods;

struct LT_Class {
    LT_Object base;
    LT_Class* superclass;
    char* name;
    size_t instance_size;
    unsigned int class_flags;
    void (*debugPrintOn)(LT_Object* obj, FILE* stream);
    LT_Class_CoreMethods* core_methods;
};

LT_DECLARE_CLASS(LT_Nil);


static inline void LT_Object_debugPrintOn(LT_Object obj, FILE* stream){
    LT_Object_class(obj)->debugPrintOn(obj, stream);
}

extern void* LT_Class_alloc(LT_Class* klass);

#define LT_Class_ALLOC(type) (type*)(LT_Class_alloc(type##_class))

extern void* LT_Class_alloc_flexible(LT_Class* klass, size_t flex);

#define LT_Class_ALLOC_FLEXIBLE(type, flex) \
    (type*)(LT_Class_alloc_flexible(type##_class, flex))

LT__END_DECLS

#endif