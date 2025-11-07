#ifndef H__ListTalk__decl_macros__
#define H__ListTalk__decl_macros__

#include <ListTalk/env_macros.h>
#include <ListTalk/value.h>
#include <ListTalk/error.h>

#define LT_DECLARE_CLASS(name)\
    extern LT_Class name##_class; \
    extern LT_Class name##_class_class; \
    typedef struct name##_s name; \
    inline name* name##_from_object(LT_Object obj){ \
        if (LT_value_class(obj) != &name##_class) { \
            LT_type_error(obj, &name##_class); \
        } \
        return (name*)LT_VALUE_POINTER_VALUE(obj); \
    }

#define LT_DEFINE_CLASS(c_name) \
    static LT_Class c_name##__class; \
    static LT_Class c_name##__class_class; \
    static LT_Class c_name##__class = { \
        .base = { \
            .klass = &c_name##__class_class, \
        }, \
    }; \
    static LT_NativeClass_Descriptor c_name##__class_descriptor; \
    \
    static LT_NativeClass_Descriptor c_name##__class_descriptor = 

#endif