#ifndef H__ListTalk__OOP__
#define H__ListTalk__OOP__

#include <ListTalk/env_macros.h>

#include <stdlib.h>

LT__CXX_GUARD_BEGIN

typedef struct LT_Object LT_Object;

#define LT_CLASS_FLAG_VAR_SIZE 1
#define LT_CLASS_FLAG_SCALAR   2

typedef struct LT_Class LT_Class;
struct LT_Object{
    LT_Class* klass;
};

typedef struct LT_Symbol LT_Symbol;
struct LT_Class {
    LT_Object base;
    LT_Class* superclass;
    char* name;
    size_t instance_size;
    unsigned int class_flags;
};

#define LT_OOP_NIL NULL
#define LT_OOP_INVALID (LT_Object*)(~((size_t)NULL))

inline LT_Class* LT_OOP_class(LT_Object* object){
    return object->klass;
}

extern LT_Object* LT_Class_alloc(LT_Class* klass);

#define LT_Class_ALLOC(type) (type*)(LT_Class_alloc(&type##_class))

LT__CXX_GUARD_END

#endif