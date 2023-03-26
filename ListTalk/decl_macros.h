#ifndef H__ListTalk__decl_macros__
#define H__ListTalk__decl_macros__

#include <ListTalk/env_macros.h>

#include <ListTalk/OOP.h>

#define LT_DECLARE_CLASS(name)\
    extern LT_Class * const name##_class; \
    typedef struct name name

#define LT_DEFINE_CLASS(c_name) \
    static LT_Class c_name##__class; \
    LT_Class* const c_name##_class = &c_name##__class; \
    static LT_Class c_name##__class =

#endif