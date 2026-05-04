/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__method_macros__
#define H__ListTalk__method_macros__

#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/error.h>

#define LT_SUBCLASS_RESPONSIBILITY_METHOD_DESCRIPTION(description) \
    description " Subclasses must implement this method."

#define LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(                 \
    c_name,                                                         \
    primitive_name,                                                 \
    description                                                     \
)                                                                  \
    LT_DEFINE_PRIMITIVE(                                            \
        c_name,                                                     \
        primitive_name,                                             \
        "(self)",                                                   \
        LT_SUBCLASS_RESPONSIBILITY_METHOD_DESCRIPTION(description)  \
    ){                                                              \
        LT_Value cursor = arguments;                                \
        LT_Value self;                                              \
        (void)tail_call_unwind_marker;                              \
                                                                    \
        LT_OBJECT_ARG(cursor, self);                                \
        LT_ARG_END(cursor);                                         \
        (void)self;                                                 \
        LT_subclass_responsibility_error();                         \
    }

#define LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_1(                 \
    c_name,                                                         \
    primitive_name,                                                 \
    arg_name,                                                       \
    description                                                     \
)                                                                  \
    LT_DEFINE_PRIMITIVE(                                            \
        c_name,                                                     \
        primitive_name,                                             \
        "(self " #arg_name ")",                                     \
        LT_SUBCLASS_RESPONSIBILITY_METHOD_DESCRIPTION(description)  \
    ){                                                              \
        LT_Value cursor = arguments;                                \
        LT_Value self;                                              \
        LT_Value arg_name;                                          \
        (void)tail_call_unwind_marker;                              \
                                                                    \
        LT_OBJECT_ARG(cursor, self);                                \
        LT_OBJECT_ARG(cursor, arg_name);                            \
        LT_ARG_END(cursor);                                         \
        (void)self;                                                 \
        (void)arg_name;                                             \
        LT_subclass_responsibility_error();                         \
    }

#endif
