/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Restart__
#define H__ListTalk__Restart__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/classes/Primitive.h>
#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Restart);

struct LT_Restart_s {
    LT_Object base;
    LT_Value name;
    LT_Value description;
    LT_Value argument_list;
    LT_Value callable;
};

#define LT_RESTART_IMPL_NAME(restart_object_name) restart_object_name##_impl

#define LT_RESTART_HEAD(restart_object_name) \
    static LT_Value LT_RESTART_IMPL_NAME(restart_object_name)(LT_Value arguments, \
                                                              LT_Value invocation_context_kind, \
                                                              LT_Value invocation_context_data, \
                                                              LT_TailCallUnwindMarker* tail_call_unwind_marker)

#define LT_DECLARE_PRIMITIVE_RESTART( \
    restart_object_name, \
    restart_name, \
    restart_argument_list, \
    restart_description \
) \
    LT_RESTART_HEAD(restart_object_name); \
    static LT_Primitive restart_object_name##_primitive = { \
        .function = LT_RESTART_IMPL_NAME(restart_object_name), \
        .flags = 0, \
        .name = restart_name, \
        .arguments = restart_argument_list, \
        .description = restart_description \
    }; \
    static LT_Restart restart_object_name = { \
        .base = {.klass = &LT_Restart_class}, \
        .name = LT_INVALID, \
        .description = LT_INVALID, \
        .argument_list = LT_INVALID, \
        .callable = LT_INVALID \
    }; \
    static void LT___init_##restart_object_name(void){ \
        LT_Restart_init_static( \
            &restart_object_name, \
            &restart_object_name##_primitive \
        ); \
    } \
    LT_REGISTER_CONSTRUCTOR(LT___init_##restart_object_name)

#define LT_DEFINE_PRIMITIVE_RESTART( \
    restart_object_name, \
    restart_name, \
    restart_argument_list, \
    restart_description \
) \
    LT_DECLARE_PRIMITIVE_RESTART( \
        restart_object_name, \
        restart_name, \
        restart_argument_list, \
        restart_description \
    ); \
    LT_RESTART_HEAD(restart_object_name)

LT_Value LT_Restart_new(LT_Value name,
                        LT_Value description,
                        LT_Value argument_list,
                        LT_Value callable);
LT_Value LT_Restart_fromClosure(LT_Value closure);
LT_Value LT_Restart_named_fromClosure(LT_Value name, LT_Value closure);
void LT_Restart_init_static(LT_Restart* restart, LT_Primitive* primitive);
LT_Value LT_Restart_from_static(LT_Restart* restart);
LT_Value LT_Restart_name(LT_Restart* restart);
LT_Value LT_Restart_description(LT_Restart* restart);
LT_Value LT_Restart_argument_list(LT_Restart* restart);
LT_Value LT_Restart_callable(LT_Restart* restart);

LT__END_DECLS

#endif
