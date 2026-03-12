/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/compiler.h>

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Macro.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/SpecialForm.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>

#define LT_COMPILER_MAX_MACROEXPAND_STEPS 256

static LT_Value fold_application(LT_Value expression,
                                 LT_Environment* lexical_environment);

static LT_Value fold_list_items(LT_Value list, LT_Environment* lexical_environment){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    LT_Value cursor = list;

    while (LT_Pair_p(cursor)){
        LT_ListBuilder_append(
            builder,
            LT_compiler_fold_expression(LT_car(cursor), lexical_environment)
        );
        cursor = LT_cdr(cursor);
    }

    if (cursor == LT_NIL){
        return LT_ListBuilder_value(builder);
    }

    return LT_ListBuilder_valueWithRest(
        builder,
        LT_compiler_fold_expression(cursor, lexical_environment)
    );
}

LT_Value LT_compiler_macroexpand(LT_Value expression,
                                 LT_Environment* lexical_environment){
    LT_Value expanded = expression;
    size_t expansion_count = 0;

    while (LT_Pair_p(expanded)){
        LT_Value operator_value = LT_compiler_fold_expression(
            LT_car(expanded),
            lexical_environment
        );
        LT_Value implementation;

        if (!LT_Macro_p(operator_value)){
            return expanded;
        }

        implementation = LT_Macro_callable(LT_Macro_from_value(operator_value));
        expanded = LT_apply(
            implementation,
            LT_cdr(expanded),
            NULL
        );
        expansion_count++;

        if (expansion_count > LT_COMPILER_MAX_MACROEXPAND_STEPS){
            LT_error("Compiler macroexpand recursion limit exceeded");
        }
    }

    return expanded;
}

static LT_Value fold_application(LT_Value expression,
                                 LT_Environment* lexical_environment){
    LT_Value folded_operator = LT_compiler_fold_expression(
        LT_car(expression),
        lexical_environment
    );

    if (folded_operator == LT_INVALID){
        return LT_INVALID;
    }

    if (LT_SpecialForm_p(folded_operator)){
        return LT_SpecialForm_expand(
            folded_operator,
            expression,
            lexical_environment
        );
    }

    if (LT_Macro_p(folded_operator)){
        LT_Value expanded = LT_compiler_macroexpand(expression, lexical_environment);
        return LT_compiler_fold_expression(expanded, lexical_environment);
    }

    return LT_cons(
        folded_operator,
        fold_list_items(LT_cdr(expression), lexical_environment)
    );
}

LT_Value LT_compiler_fold_expression(LT_Value expression,
                                     LT_Environment* lexical_environment){
    LT_Value value;
    unsigned int flags;

    if (LT_Symbol_p(expression)){
        if (!LT_Environment_lookup(
            lexical_environment,
            expression,
            &value,
            &flags
        )){
            return LT_INVALID;
        }
        if ((flags & LT_ENV_BINDING_FLAG_CONSTANT) == 0){
            return LT_INVALID;
        }
        return value;
    }

    if (LT_Pair_p(expression)){
        return fold_application(expression, lexical_environment);
    }

    return expression;
}
