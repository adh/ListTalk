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

#include <string.h>

#define LT_COMPILER_MAX_MACROEXPAND_STEPS 256

static LT_Value constant_value_to_expression(
    LT_Value value,
    LT_Environment* lexical_environment
);
static LT_Value fold_application(LT_Value expression,
                                 LT_Environment* lexical_environment);
static int is_quote_special_form_value(LT_Value value);
static int compiler_list_p(LT_Value value);
static LT_Value immutable_list_with_rest(
    LT_Value first,
    LT_Value second,
    LT_Value rest
);

static int compiler_list_p(LT_Value value){
    return LT_Value_is_instance_of(value, LT_STATIC_CLASS(LT_List));
}

static LT_Value immutable_list_with_rest(
    LT_Value first,
    LT_Value second,
    LT_Value rest
){
    LT_Value values[2] = {first, second};
    return LT_ImmutableList_new_with_tail(2, values, rest);
}

static LT_Value fold_list_items(LT_Value list, LT_Environment* lexical_environment){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    LT_Value cursor = list;
    LT_Value result;

    while (compiler_list_p(cursor)){
        LT_ListBuilder_append(
            builder,
            LT_compiler_fold_expression(LT_car(cursor), lexical_environment)
        );
        cursor = LT_cdr(cursor);
    }

    if (cursor == LT_NIL){
        result = LT_ListBuilder_value(builder);
        return LT_ImmutableList_fromList(result);
    }

    result = LT_ListBuilder_valueWithRest(
        builder,
        LT_compiler_fold_expression(cursor, lexical_environment)
    );
    return LT_ImmutableList_fromList(result);
}

static int is_quote_special_form_value(LT_Value value){
    LT_SpecialForm* special_form;
    char* name;

    if (!LT_SpecialForm_p(value)){
        return 0;
    }
    special_form = LT_SpecialForm_from_value(value);
    name = LT_SpecialForm_name(special_form);
    return name != NULL && strcmp(name, "quote") == 0;
}

static LT_Value constant_value_to_expression(
    LT_Value value,
    LT_Environment* lexical_environment
){
    if (!compiler_list_p(value)){
        return value;
    }

    return immutable_list_with_rest(
        LT_compiler_fold_expression(
            LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "quote"),
            lexical_environment
        ),
        value,
        LT_NIL
    );
}

LT_Value LT_compiler_macroexpand(LT_Value expression,
                                 LT_Environment* lexical_environment){
    LT_Value expanded = expression;
    size_t expansion_count = 0;

    while (compiler_list_p(expanded)){
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

    if (LT_SpecialForm_p(folded_operator)){
        LT_Value expanded = LT_SpecialForm_expand(
            folded_operator,
            expression,
            lexical_environment
        );

        if (expanded == LT_INVALID){
            return LT_ImmutableList_fromList(expression);
        }
        return LT_ImmutableList_fromList(expanded);
    }

    if (LT_Macro_p(folded_operator)){
        LT_Value expanded = LT_compiler_macroexpand(expression, lexical_environment);
        return LT_compiler_fold_expression(expanded, lexical_environment);
    }

    {
        LT_Value folded_arguments = fold_list_items(
            LT_cdr(expression),
            lexical_environment
        );

        if (LT_Primitive_p(folded_operator)
            && (LT_Primitive_flags(
                LT_Primitive_from_value(folded_operator)
            ) & LT_PRIMITIVE_FLAG_PURE) != 0){
            LT_ListBuilder* constant_arguments_builder = LT_ListBuilder_new();
            LT_Value cursor = folded_arguments;

            while (compiler_list_p(cursor)){
                LT_Value constant_value = LT_compiler_expression_constant_value(
                    LT_car(cursor),
                    lexical_environment
                );

                if (constant_value == LT_INVALID){
                    return LT_ImmutableList_new_with_tail(
                        1,
                        &folded_operator,
                        folded_arguments
                    );
                }
                LT_ListBuilder_append(constant_arguments_builder, constant_value);
                cursor = LT_cdr(cursor);
            }

            if (cursor == LT_NIL){
                LT_Value constant_result = LT_Primitive_call(
                    folded_operator,
                    LT_ListBuilder_value(constant_arguments_builder),
                    NULL
                );
                return constant_value_to_expression(
                    constant_result,
                    lexical_environment
                );
            }
        }

        return LT_ImmutableList_new_with_tail(
            1,
            &folded_operator,
            folded_arguments
        );
    }
}

LT_Value LT_compiler_expression_constant_value(
    LT_Value folded_expression,
    LT_Environment* lexical_environment
){
    LT_Value value;
    unsigned int flags;

    if (LT_Symbol_p(folded_expression)){
        if (!LT_Environment_lookup(
            lexical_environment,
            folded_expression,
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

    if (compiler_list_p(folded_expression)){
        LT_Value cursor = folded_expression;
        LT_Value operator_value;

        operator_value = LT_car(cursor);
        cursor = LT_cdr(cursor);

        if (!is_quote_special_form_value(operator_value)){
            return LT_INVALID;
        }
        if (!compiler_list_p(cursor)){
            return LT_INVALID;
        }
        value = LT_car(cursor);
        cursor = LT_cdr(cursor);
        if (cursor != LT_NIL){
            return LT_INVALID;
        }
        return value;
    }

    return folded_expression;
}

LT_Value LT_compiler_fold_expression(LT_Value expression,
                                     LT_Environment* lexical_environment){
    LT_Value constant_value;

    if (LT_Symbol_p(expression)){
        constant_value = LT_compiler_expression_constant_value(
            expression,
            lexical_environment
        );
        if (constant_value == LT_INVALID){
            return expression;
        }
        return constant_value_to_expression(constant_value, lexical_environment);
    }

    if (compiler_list_p(expression)){
        return fold_application(expression, lexical_environment);
    }

    return expression;
}
