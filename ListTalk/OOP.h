#ifndef H__ListTalk__OOP__
#define H__ListTalk__OOP__

#include <stdlib.h>

typedef struct LT_Object LT_Object;

#define LT_CLASS_FLAG_VAR_SIZE 1

typedef struct LT_Class LT_Class;
struct LT_Object{
    LT_Class* klass;
};

typedef struct LT_Symbol LT_Symbol;
struct LT_Class {
    LT_Object base;
    LT_Class* superclass;
    LT_Symbol* name;
    size_t instance_size;
    int class_flags;
};

#define LT_OOP_NIL NULL
#define LT_OOP_INVALID (LT_Object*)(~((size_t)NULL))

inline LT_Class* LT_OOP_class(LT_Object* object){
    return object->klass;
}


#endif