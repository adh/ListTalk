/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__arg_macros__
#define H__ListTalk__arg_macros__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/value.h>

#include <stdint.h>

#define LT_OBJECT_ARG(al, name)                                      \
    do {                                                             \
        if ((al) == LT_NIL){                                   \
            LT_error("Required argument missing: " #name);           \
        }                                                            \
        if (!LT_Value_is_pair((al))){                                \
            LT_error("Malformed argument list while parsing " #name); \
        }                                                            \
        (name) = LT_car((al));                                       \
        (al) = LT_cdr((al));                                         \
    } while (0)

#define LT_DISCARD_ARG(al, name)                                     \
    do {                                                             \
        if ((al) == LT_NIL){                                   \
            LT_error("Required argument missing: " #name);           \
        }                                                            \
        if (!LT_Value_is_pair((al))){                                \
            LT_error("Malformed argument list while parsing " #name); \
        }                                                            \
        (al) = LT_cdr((al));                                         \
    } while (0)

#define LT_OBJECT_ARG_OPT(al, name, default_value)                   \
    do {                                                             \
        if ((al) == LT_NIL){                                   \
            (name) = (default_value);                                \
        } else {                                                     \
            LT_OBJECT_ARG((al), (name));                             \
        }                                                            \
    } while (0)

#define LT_GENERIC_ARG(al, name, c_type, conv)                       \
    do {                                                             \
        LT_Value LT___arg_tmp;                                       \
        LT_OBJECT_ARG((al), LT___arg_tmp);                           \
        (name) = (c_type)(conv)(LT___arg_tmp);                       \
    } while (0)

#define LT_GENERIC_ARG_OPT(al, name, default_value, c_type, conv)    \
    do {                                                             \
        if ((al) == LT_NIL){                                   \
            (name) = (default_value);                                \
        } else {                                                     \
            LT_GENERIC_ARG((al), (name), c_type, conv);              \
        }                                                            \
    } while (0)

#define LT_FIXNUM_ARG(al, name)                                      \
    do {                                                             \
        LT_Value LT___arg_tmp;                                       \
        LT_OBJECT_ARG((al), LT___arg_tmp);                           \
        if (!LT_Value_is_fixnum(LT___arg_tmp)){                      \
            LT_error("Expected fixnum argument: " #name);            \
        }                                                            \
        (name) = LT_Value_fixnum_value(LT___arg_tmp);                \
    } while (0)

#define LT_ARG_END(al)                                               \
    do {                                                             \
        if ((al) != LT_NIL){                                   \
            LT_error("Too many arguments");                          \
        }                                                            \
    } while (0)

#define LT_ARG_REST(al, rest)                                        \
    do {                                                             \
        (rest) = (al);                                               \
    } while (0)

#endif
