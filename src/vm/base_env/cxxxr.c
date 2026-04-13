/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/classes/Pair.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/error.h>

#include <string.h>

static LT_Value apply_cxr_path(LT_Value value, const char* path){
    size_t index = strlen(path);

    while (index > 0){
        char op = path[index - 1];
        index--;

        if (!LT_Pair_p(value)){
            LT_error("cxxxr primitive expects list at each step");
        }
        if (op == 'a'){
            value = LT_car(value);
        } else if (op == 'd'){
            value = LT_cdr(value);
        } else {
            LT_error("Invalid cxxxr primitive path");
        }
    }

    return value;
}

#define LT_DEFINE_CXR_PRIMITIVE(c_name, symbol_name, path_spec)            \
    LT_DEFINE_PRIMITIVE(                                                    \
        c_name,                                                             \
        symbol_name,                                                        \
        "(pair)",                                                          \
        "Return pair navigation primitive result."                         \
    ){                                                                      \
        LT_Value cursor = arguments;                                        \
        LT_Value value;                                                     \
                                                                            \
        LT_OBJECT_ARG(cursor, value);                                       \
        LT_ARG_END(cursor);                                                 \
        return apply_cxr_path(value, path_spec);              \
    }

LT_DEFINE_CXR_PRIMITIVE(primitive_caar, "caar", "aa")
LT_DEFINE_CXR_PRIMITIVE(primitive_cadr, "cadr", "ad")
LT_DEFINE_CXR_PRIMITIVE(primitive_cdar, "cdar", "da")
LT_DEFINE_CXR_PRIMITIVE(primitive_cddr, "cddr", "dd")

LT_DEFINE_CXR_PRIMITIVE(primitive_caaar, "caaar", "aaa")
LT_DEFINE_CXR_PRIMITIVE(primitive_caadr, "caadr", "aad")
LT_DEFINE_CXR_PRIMITIVE(primitive_cadar, "cadar", "ada")
LT_DEFINE_CXR_PRIMITIVE(primitive_caddr, "caddr", "add")
LT_DEFINE_CXR_PRIMITIVE(primitive_cdaar, "cdaar", "daa")
LT_DEFINE_CXR_PRIMITIVE(primitive_cdadr, "cdadr", "dad")
LT_DEFINE_CXR_PRIMITIVE(primitive_cddar, "cddar", "dda")
LT_DEFINE_CXR_PRIMITIVE(primitive_cdddr, "cdddr", "ddd")

LT_DEFINE_CXR_PRIMITIVE(primitive_caaaar, "caaaar", "aaaa")
LT_DEFINE_CXR_PRIMITIVE(primitive_caaadr, "caaadr", "aaad")
LT_DEFINE_CXR_PRIMITIVE(primitive_caadar, "caadar", "aada")
LT_DEFINE_CXR_PRIMITIVE(primitive_caaddr, "caaddr", "aadd")
LT_DEFINE_CXR_PRIMITIVE(primitive_cadaar, "cadaar", "adaa")
LT_DEFINE_CXR_PRIMITIVE(primitive_cadadr, "cadadr", "adad")
LT_DEFINE_CXR_PRIMITIVE(primitive_caddar, "caddar", "adda")
LT_DEFINE_CXR_PRIMITIVE(primitive_cadddr, "cadddr", "addd")
LT_DEFINE_CXR_PRIMITIVE(primitive_cdaaar, "cdaaar", "daaa")
LT_DEFINE_CXR_PRIMITIVE(primitive_cdaadr, "cdaadr", "daad")
LT_DEFINE_CXR_PRIMITIVE(primitive_cdadar, "cdadar", "dada")
LT_DEFINE_CXR_PRIMITIVE(primitive_cdaddr, "cdaddr", "dadd")
LT_DEFINE_CXR_PRIMITIVE(primitive_cddaar, "cddaar", "ddaa")
LT_DEFINE_CXR_PRIMITIVE(primitive_cddadr, "cddadr", "ddad")
LT_DEFINE_CXR_PRIMITIVE(primitive_cdddar, "cdddar", "ddda")
LT_DEFINE_CXR_PRIMITIVE(primitive_cddddr, "cddddr", "dddd")

void LT_base_env_bind_cxxxr(LT_Environment* environment){
    LT_base_env_bind_static_primitive(environment, &primitive_caar);
    LT_base_env_bind_static_primitive(environment, &primitive_cadr);
    LT_base_env_bind_static_primitive(environment, &primitive_cdar);
    LT_base_env_bind_static_primitive(environment, &primitive_cddr);

    LT_base_env_bind_static_primitive(environment, &primitive_caaar);
    LT_base_env_bind_static_primitive(environment, &primitive_caadr);
    LT_base_env_bind_static_primitive(environment, &primitive_cadar);
    LT_base_env_bind_static_primitive(environment, &primitive_caddr);
    LT_base_env_bind_static_primitive(environment, &primitive_cdaar);
    LT_base_env_bind_static_primitive(environment, &primitive_cdadr);
    LT_base_env_bind_static_primitive(environment, &primitive_cddar);
    LT_base_env_bind_static_primitive(environment, &primitive_cdddr);

    LT_base_env_bind_static_primitive(environment, &primitive_caaaar);
    LT_base_env_bind_static_primitive(environment, &primitive_caaadr);
    LT_base_env_bind_static_primitive(environment, &primitive_caadar);
    LT_base_env_bind_static_primitive(environment, &primitive_caaddr);
    LT_base_env_bind_static_primitive(environment, &primitive_cadaar);
    LT_base_env_bind_static_primitive(environment, &primitive_cadadr);
    LT_base_env_bind_static_primitive(environment, &primitive_caddar);
    LT_base_env_bind_static_primitive(environment, &primitive_cadddr);
    LT_base_env_bind_static_primitive(environment, &primitive_cdaaar);
    LT_base_env_bind_static_primitive(environment, &primitive_cdaadr);
    LT_base_env_bind_static_primitive(environment, &primitive_cdadar);
    LT_base_env_bind_static_primitive(environment, &primitive_cdaddr);
    LT_base_env_bind_static_primitive(environment, &primitive_cddaar);
    LT_base_env_bind_static_primitive(environment, &primitive_cddadr);
    LT_base_env_bind_static_primitive(environment, &primitive_cdddar);
    LT_base_env_bind_static_primitive(environment, &primitive_cddddr);
}
