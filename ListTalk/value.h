#ifndef H__ListTalk__value__
#define H__ListTalk__value__

#include <ListTalk/env_macros.h>

LT__BEGIN_DECLS

typedef uintptr_t LT_Value;

#define LT_VALUE_IS_IMMEDIATE(obj) (((uintptr_t)(obj) >> 63) != 0)
#define LT_VALUE_IS_POINTER(obj) (((uintptr_t)(obj) >> 63) == 0)
#define LT_VALUE_IMMEDIATE_TAG(obj) (((uintptr_t)(obj) >> 56) & 0xff)
#define LT_VALUE_IMMEDIATE_VALUE(obj) ((uintptr_t)(obj) & 0x00ffffffffffffffULL)
#define LT_VALUE_POINTER_TAG(obj) ((uintptr_t)(obj) & 0x7)
#define LT_VALUE_POINTER_VALUE(obj) ((void*)((uintptr_t)(obj) & ~0x7))
#define LT_VALUE_OBJECT_VALUE(obj) \
    ((LT_Object*)((uintptr_t)(obj) & ~0x7))

#define LT_VALUE_POINTER_TAG_OBJECT 0x0
#define LT_VALUE_POINTER_TAG_IMMUTABLE_LIST 0x1
#define LT_VALUE_POINTER_TAG_PAIR 0x2
#define LT_VALUE_POINTER_TAG_CLOSURE 0x4
#define LT_VALUE_POINTER_TAG_PRIMITIVE 0x5
#define LT_VALUE_POINTER_TAG_MACRO 0x6
#define LT_VALUE_POINTER_TAG_SPECIAL_FORM 0x7

#define LT_VALUE_IMMEDIATE_TAG_NIL 0x80
#define LT_VALUE_IMMEDIATE_TAG_SYMBOL 0x81
#define LT_VALUE_IMMEDIATE_TAG_BOOLEAN 0x82
#define LT_VALUE_IMMEDIATE_TAG_FIXNUM 0x83
#define LT_VALUE_IMMEDIATE_TAG_CHARACTER 0x84
#define LT_VALUE_IMMEDIATE_TAG_UINT8 0x85
#define LT_VALUE_IMMEDIATE_TAG_UINT16 0x86
#define LT_VALUE_IMMEDIATE_TAG_UINT32 0x87
#define LT_VALUE_IMMEDIATE_TAG_FLOAT 0x88
#define LT_VALUE_IMMEDIATE_TAG_INT8 0x89
#define LT_VALUE_IMMEDIATE_TAG_INT16 0x8a
#define LT_VALUE_IMMEDIATE_TAG_INT32 0x8b
#define LT_VALUE_IMMEDIATE_TAG_SMALL_FRACTION 0x8c
#define LT_VALUE_IMMEDIATE_TAG_IS_SHORT_STRING(tag) ((tag) & 0xf8 == 0x90)
#define LT_VALUE_IMMEDIATE_TAG_SHORT_STRING_LENGTH(tag) ((tag) & 0x7)
#define LT_VALUE_IMMEDIATE_TAG_INVALID 0xbf
#define LT_VALUE_IMMEDIATE_TAG_IS_FLONUM(tag) ((tag) & 0xc0 == 0xc0)

#define LT_VALUE_MAKE_IMMEDIATE(tag, value) \
    ((LT_Value)((((uintptr_t)(tag) & 0xff) << 56) \
    | ((uintptr_t)(value) & 0x00ffffffffffffffULL) \
    | 0x8000000000000000ULL))

#define LT_VALUE_NIL \
    LT_VALUE_MAKE_IMMEDIATE(LT_VALUE_IMMEDIATE_TAG_NIL, 0)

typedef struct LT_Class_s LT_Class;

typedef struct LT_Object_s {
    LT_Class* klass;
} LT_Object;

extern LT_Class* const LT__Immediate_classes[];
extern LT_Class* const LT__Pointer_classes[];
extern LT_Class LT_Float_class;

static inline LT_Class* LT_Value_class(LT_Value value){
    if (LT_VALUE_IS_IMMEDIATE(value)){
        if (LT_VALUE_IMMEDIATE_TAG_IS_FLONUM(LT_VALUE_IMMEDIATE_TAG(value))){
            return &LT_Float_class;
        }
        return LT__Immediate_classes[LT_VALUE_IMMEDIATE_TAG(value) & 0x3f];
    }
    if (LT_VALUE_POINTER_TAG(value) != LT_VALUE_POINTER_TAG_OBJECT){
        return LT__Pointer_classes[LT_VALUE_POINTER_TAG(value)];
    }
    return LT_VALUE_OBJECT_VALUE(value)->klass;
}


LT__END_DECLS

#endif