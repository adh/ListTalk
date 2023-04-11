#ifndef H__ListTalk__OOP__
#define H__ListTalk__OOP__

#include <ListTalk/env_macros.h>
#include <ListTalk/decl_macros.h>

#include <stdlib.h>
#include <stdio.h>

LT__BEGIN_DECLS

typedef struct LT_Object LT_Object;

#define LT_CLASS_FLAG_FLEXIBLE 1
#define LT_CLASS_FLAG_SCALAR   2

typedef struct LT_Class LT_Class;
struct LT_Object{
    LT_Class* klass;
};

typedef struct LT_Symbol LT_Symbol;

typedef struct LT_Class_CoreMethods LT_Class_CoreMethods;

struct LT_Class {
    LT_Object base;
    LT_Class* superclass;
    char* name;
    size_t instance_size;
    unsigned int class_flags;
    void (*printOn)(LT_Object* obj, FILE* stream); /* TODO: temporary */
    LT_Class_CoreMethods* core_methods;
};


#define LT_OOP_NIL NULL
#define LT_OOP_INVALID (LT_Object*)(~((size_t)NULL))

LT_DECLARE_CLASS(LT_Nil);

inline LT_Class* LT_Object_class(LT_Object* object){
    if (object == LT_OOP_NIL){
        return LT_Nil_class;
    }
    return object->klass;
}

inline void LT_Object_printOn(LT_Object* obj, FILE* stream){
    LT_Object_class(obj)->printOn(obj, stream);
}

extern LT_Object* LT_Class_alloc(LT_Class* klass);

#define LT_Class_ALLOC(type) (type*)(LT_Class_alloc(type##_class))

extern LT_Object* LT_Class_alloc_flexible(LT_Class* klass, size_t flex);

#define LT_Class_ALLOC_FLEXIBLE(type, flex) \
    (type*)(LT_Class_alloc_flexible(type##_class, flex))

LT__END_DECLS

#endif