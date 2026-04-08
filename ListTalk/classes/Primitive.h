/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Primitive__
#define H__ListTalk__Primitive__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/vm/tail_call.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Primitive);

typedef LT_Value(*LT_Primitive_Func)(LT_Value arguments,
                                     LT_Value invocation_context_kind,
                                     LT_Value invocation_context_data,
                                     LT_TailCallUnwindMarker* tail_call_unwind_marker);

#define LT_PRIMITIVE_FLAG_PURE 0x1

struct LT_Primitive_s {
    LT_Primitive_Func function;
    unsigned int flags;
    char* name;
    char* arguments;
    char* description;
};

#define LT_PRIMITIVE_IMPL_NAME(primitive_object_name) primitive_object_name##_impl

#define LT_PRIMITIVE_HEAD(primitive_object_name) \
    static LT_Value LT_PRIMITIVE_IMPL_NAME(primitive_object_name)(LT_Value arguments, \
                                                                  LT_Value invocation_context_kind, \
                                                                  LT_Value invocation_context_data, \
                                                                  LT_TailCallUnwindMarker* tail_call_unwind_marker)

#define LT_DECLARE_PRIMITIVE(primitive_object_name, primitive_name, primitive_arguments, primitive_description) \
    LT_DECLARE_PRIMITIVE_FLAGS( \
        primitive_object_name, \
        primitive_name, \
        primitive_arguments, \
        primitive_description, \
        0 \
    )

#define LT_DECLARE_PRIMITIVE_FLAGS( \
    primitive_object_name, \
    primitive_name, \
    primitive_arguments, \
    primitive_description, \
    primitive_flags \
) \
    LT_PRIMITIVE_HEAD(primitive_object_name); \
    static LT_Primitive primitive_object_name = { \
        .function = LT_PRIMITIVE_IMPL_NAME(primitive_object_name), \
        .flags = primitive_flags, \
        .name = primitive_name, \
        .arguments = primitive_arguments, \
        .description = primitive_description \
    }

#define LT_DEFINE_PRIMITIVE(primitive_object_name, primitive_name, primitive_arguments, primitive_description) \
    LT_DECLARE_PRIMITIVE(primitive_object_name, primitive_name, primitive_arguments, primitive_description); \
    LT_PRIMITIVE_HEAD(primitive_object_name)

#define LT_DEFINE_PRIMITIVE_FLAGS( \
    primitive_object_name, \
    primitive_name, \
    primitive_arguments, \
    primitive_description, \
    primitive_flags \
) \
    LT_DECLARE_PRIMITIVE_FLAGS( \
        primitive_object_name, \
        primitive_name, \
        primitive_arguments, \
        primitive_description, \
        primitive_flags \
    ); \
    LT_PRIMITIVE_HEAD(primitive_object_name)

LT_Value LT_Primitive_new(char* name,
                          char* arguments,
                          char* description,
                          LT_Primitive_Func function);
LT_Value LT_Primitive_new_with_flags(char* name,
                                     char* arguments,
                                     char* description,
                                     LT_Primitive_Func function,
                                     unsigned int flags);
LT_Value LT_Primitive_from_static(LT_Primitive* primitive);
char* LT_Primitive_name(LT_Primitive* primitive);
char* LT_Primitive_arguments(LT_Primitive* primitive);
char* LT_Primitive_description(LT_Primitive* primitive);
LT_Primitive_Func LT_Primitive_function(LT_Primitive* primitive);
unsigned int LT_Primitive_flags(LT_Primitive* primitive);
LT_Value LT_Primitive_call(LT_Value primitive,
                           LT_Value arguments,
                           LT_Value invocation_context_kind,
                           LT_Value invocation_context_data,
                           LT_TailCallUnwindMarker* tail_call_unwind_marker);

LT__END_DECLS

#endif
