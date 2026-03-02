#ifndef H__ListTalk__decl_macros__
#define H__ListTalk__decl_macros__

#include <ListTalk/env_macros.h>
#include <ListTalk/value.h>
#include <ListTalk/error.h>
#include <ListTalk/Class.h>

#define LT_DECLARE_CLASS(name) \
    typedef struct name##_s name; \
    extern LT_Class name##_class; \
    extern LT_Class name##_class_class; \
    static inline name* name##_from_object(LT_Value value){ \
        if (LT_Value_class(value) != &name##_class) { \
            LT_type_error(value, &name##_class); \
        } \
        return (name*)LT_VALUE_POINTER_VALUE(value); \
    }

#define LT_DEFINE_CLASS(c_name) \
    static LT_Class_Descriptor c_name##_class_descriptor; \
    LT_Class c_name##_class_class = { \
        .base = {.klass = &LT_Class_class_class}, \
        .instance_size = sizeof(LT_Class), \
    }; \
    LT_Class c_name##_class = { \
        .base = {.klass = &c_name##_class_class}, \
        .native_descriptor = &c_name##_class_descriptor, \
    }; \
    static void LT___init_##c_name(void) __attribute__((constructor)); \
    static void LT___init_##c_name(void){ \
        LT_init_native_class(&c_name##_class); \
    } \
    static LT_Class_Descriptor c_name##_class_descriptor =

#endif
