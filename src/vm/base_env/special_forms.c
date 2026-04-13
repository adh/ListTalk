/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Closure.h>
#include <ListTalk/classes/Macro.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/conditions.h>
#include <ListTalk/vm/compiler.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/throw_catch.h>

#include <string.h>

static LT_Value fold_argument_list(LT_Value arguments,
                                   LT_Environment* environment){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    LT_Value cursor = arguments;

    while (LT_Pair_p(cursor)){
        LT_ListBuilder_append(
            builder,
            LT_compiler_fold_expression(LT_car(cursor), environment)
        );
        cursor = LT_cdr(cursor);
    }

    if (cursor == LT_NIL){
        return LT_ListBuilder_value(builder);
    }

    return LT_ListBuilder_valueWithRest(
        builder,
        LT_compiler_fold_expression(cursor, environment)
    );
}

static LT_Value fold_operator_special_form_value(LT_Value form,
                                                 LT_Environment* environment){
    LT_Value value = LT_compiler_fold_expression(LT_car(form), environment);

    if (!LT_SpecialForm_p(value)){
        return LT_INVALID;
    }
    return value;
}

static LT_Value expand_special_form_quote(LT_Value form,
                                          LT_Environment* environment){
    LT_Value special_form_value = fold_operator_special_form_value(
        form,
        environment
    );
    LT_SpecialForm* special_form;

    if (special_form_value == LT_INVALID){
        return LT_INVALID;
    }
    special_form = LT_SpecialForm_from_value(special_form_value);

    return LT_cons(
        LT_SpecialForm_from_static(special_form),
        LT_cdr(form)
    );
}

static LT_Value expand_special_form_lambda(LT_Value form,
                                           LT_Environment* environment){
    LT_Value special_form_value = fold_operator_special_form_value(
        form,
        environment
    );
    LT_SpecialForm* special_form;
    LT_Value cursor = LT_cdr(form);
    LT_Value name = LT_NIL;
    LT_Value folded_name = LT_NIL;
    LT_Value parameters = LT_NIL;
    LT_Value body = LT_NIL;

    if (special_form_value == LT_INVALID){
        return LT_INVALID;
    }
    special_form = LT_SpecialForm_from_value(special_form_value);

    if (LT_Pair_p(cursor)){
        name = LT_car(cursor);
        folded_name = LT_compiler_fold_expression(name, environment);
        cursor = LT_cdr(cursor);
    }
    if (LT_Pair_p(cursor)){
        parameters = LT_car(cursor);
        body = fold_argument_list(LT_cdr(cursor), environment);
    }

    return LT_cons(
        LT_SpecialForm_from_static(special_form),
        LT_cons(folded_name, LT_cons(parameters, body))
    );
}

static LT_Value expand_special_form_define(LT_Value form,
                                           LT_Environment* environment){
    LT_Value special_form_value = fold_operator_special_form_value(
        form,
        environment
    );
    LT_SpecialForm* special_form;
    LT_Value arguments = LT_cdr(form);
    LT_Value symbol = LT_NIL;
    LT_Value value_and_rest = LT_NIL;

    if (special_form_value == LT_INVALID){
        return LT_INVALID;
    }
    special_form = LT_SpecialForm_from_value(special_form_value);

    if (LT_Pair_p(arguments)){
        symbol = LT_car(arguments);
        value_and_rest = fold_argument_list(LT_cdr(arguments), environment);
    }

    return LT_cons(
        LT_SpecialForm_from_static(special_form),
        LT_cons(symbol, value_and_rest)
    );
}

static LT_Value expand_special_form_set_bang(LT_Value form,
                                             LT_Environment* environment){
    LT_Value special_form_value = fold_operator_special_form_value(
        form,
        environment
    );
    LT_SpecialForm* special_form;
    LT_Value arguments = LT_cdr(form);
    LT_Value symbol = LT_NIL;
    LT_Value value_and_rest = LT_NIL;

    if (special_form_value == LT_INVALID){
        return LT_INVALID;
    }
    special_form = LT_SpecialForm_from_value(special_form_value);

    if (LT_Pair_p(arguments)){
        symbol = LT_car(arguments);
        value_and_rest = fold_argument_list(LT_cdr(arguments), environment);
    }

    return LT_cons(
        LT_SpecialForm_from_static(special_form),
        LT_cons(symbol, value_and_rest)
    );
}

static LT_Value expand_special_form_default(LT_Value form,
                                            LT_Environment* environment){
    LT_Value special_form_value = fold_operator_special_form_value(
        form,
        environment
    );
    LT_SpecialForm* special_form;

    if (special_form_value == LT_INVALID){
        return LT_INVALID;
    }
    special_form = LT_SpecialForm_from_value(special_form_value);

    return LT_cons(
        LT_SpecialForm_from_static(special_form),
        fold_argument_list(LT_cdr(form), environment)
    );
}


static LT_Value special_form_quote(LT_Value arguments,
                                   LT_Environment* environment,
                                   LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value value;
    (void)environment;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return value;
}

static LT_Value special_form_quasiquote(LT_Value arguments,
                                        LT_Environment* environment,
                                        LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_quasiquote(value, environment);
}

static LT_Value special_form_lambda(LT_Value arguments,
                                    LT_Environment* environment,
                                    LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value name_expression;
    LT_Value name;
    LT_Value parameters;
    LT_Value body;
    LT_Value parameter_cursor;

    LT_OBJECT_ARG(cursor, name_expression);
    LT_OBJECT_ARG(cursor, parameters);
    LT_ARG_REST(cursor, body);
    if (body == LT_NIL){
        LT_error("Special form %lambda expects body");
    }
    (void)tail_call_unwind_marker;
    if (name_expression == LT_NIL){
        name = LT_NIL;
    } else {
        name = LT_eval(name_expression, environment, NULL);
    }

    parameter_cursor = parameters;
    if (LT_Symbol_p(parameter_cursor)){
        return LT_Closure_new(name, parameters, body, environment);
    }

    while (LT_Pair_p(parameter_cursor)){
        LT_Value parameter;
        parameter = LT_car(parameter_cursor);
        if (!LT_Symbol_p(parameter)){
            LT_error("%lambda parameter must be symbol");
        }
        parameter_cursor = LT_cdr(parameter_cursor);
    }
    if (parameter_cursor != LT_NIL && !LT_Symbol_p(parameter_cursor)){
        LT_error("%lambda parameters must be proper or dotted with symbol");
    }

    return LT_Closure_new(name, parameters, body, environment);
}

static LT_Value special_form_if(LT_Value arguments,
                                LT_Environment* environment,
                                LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value condition_expression;
    LT_Value then_expression;
    LT_Value else_expression = LT_NIL;
    LT_Value condition_value;

    LT_OBJECT_ARG(cursor, condition_expression);
    LT_OBJECT_ARG(cursor, then_expression);
    LT_OBJECT_ARG_OPT(cursor, else_expression, LT_NIL);
    LT_ARG_END(cursor);

    condition_value = LT_eval(condition_expression, environment, NULL);
    if (LT_Value_truthy_p(condition_value)){
        return LT_eval(then_expression, environment, tail_call_unwind_marker);
    }
    if (else_expression == LT_NIL){
        return LT_NIL;
    }
    return LT_eval(else_expression, environment, tail_call_unwind_marker);
}

static LT_Value special_form_and(LT_Value arguments,
                                 LT_Environment* environment,
                                 LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;

    if (cursor == LT_NIL){
        return LT_TRUE;
    }

    while (cursor != LT_NIL){
        LT_Value expression;
        LT_Value next_cursor;
        LT_Value value;

        if (!LT_Pair_p(cursor)){
            LT_error("Special form and expects proper list of forms");
        }

        expression = LT_car(cursor);
        next_cursor = LT_cdr(cursor);
        value = LT_eval(
            expression,
            environment,
            (next_cursor == LT_NIL) ? tail_call_unwind_marker : NULL
        );

        if (!LT_Value_truthy_p(value)){
            return value;
        }

        if (next_cursor == LT_NIL){
            return value;
        }
        cursor = next_cursor;
    }

    return LT_TRUE;
}

static LT_Value special_form_or(LT_Value arguments,
                                LT_Environment* environment,
                                LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;

    if (cursor == LT_NIL){
        return LT_FALSE;
    }

    while (cursor != LT_NIL){
        LT_Value expression;
        LT_Value next_cursor;
        LT_Value value;

        if (!LT_Pair_p(cursor)){
            LT_error("Special form or expects proper list of forms");
        }

        expression = LT_car(cursor);
        next_cursor = LT_cdr(cursor);
        value = LT_eval(
            expression,
            environment,
            (next_cursor == LT_NIL) ? tail_call_unwind_marker : NULL
        );

        if (LT_Value_truthy_p(value) || next_cursor == LT_NIL){
            return value;
        }
        cursor = next_cursor;
    }

    return LT_FALSE;
}

static int cond_else_clause_p(LT_Value test_expression){
    if (!LT_Symbol_p(test_expression)){
        return 0;
    }

    return strcmp(
        LT_Symbol_name(LT_Symbol_from_value(test_expression)),
        "else"
    ) == 0;
}

static LT_Value special_form_cond(LT_Value arguments,
                                  LT_Environment* environment,
                                  LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;

    while (cursor != LT_NIL){
        LT_Value clause;
        LT_Value test_expression;
        LT_Value body;
        LT_Value test_value;

        if (!LT_Pair_p(cursor)){
            LT_error("Special form cond expects proper list of clauses");
        }

        clause = LT_car(cursor);
        if (!LT_Pair_p(clause)){
            LT_error("Special form cond clause must be list");
        }

        test_expression = LT_car(clause);
        body = LT_cdr(clause);

        if (cond_else_clause_p(test_expression)){
            if (body == LT_NIL){
                LT_error("Special form cond else clause expects body");
            }
            return LT_eval_sequence(body, environment, tail_call_unwind_marker);
        }

        test_value = LT_eval(test_expression, environment, NULL);
        if (LT_Value_truthy_p(test_value)){
            if (body == LT_NIL){
                return test_value;
            }
            return LT_eval_sequence(body, environment, tail_call_unwind_marker);
        }

        cursor = LT_cdr(cursor);
    }

    return LT_FALSE;
}

static LT_Value special_form_begin(LT_Value arguments,
                                   LT_Environment* environment,
                                   LT_TailCallUnwindMarker* tail_call_unwind_marker){
    return LT_eval_sequence(arguments, environment, tail_call_unwind_marker);
}

static LT_Value special_form_let(LT_Value arguments,
                                 LT_Environment* environment,
                                 LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value bindings;
    LT_Value body;
    LT_Value binding_cursor;
    LT_ListBuilder* symbols = LT_ListBuilder_new();
    LT_ListBuilder* values = LT_ListBuilder_new();
    LT_Environment* let_environment;

    LT_OBJECT_ARG(cursor, bindings);
    LT_ARG_REST(cursor, body);
    if (body == LT_NIL){
        LT_error("Special form let expects body");
    }

    binding_cursor = bindings;
    while (binding_cursor != LT_NIL){
        LT_Value binding;
        LT_Value binding_form_cursor;
        LT_Value symbol;
        LT_Value value_expression;
        LT_Value value;

        if (!LT_Pair_p(binding_cursor)){
            LT_error("Special form let expects proper binding list");
        }

        binding = LT_car(binding_cursor);
        if (!LT_Pair_p(binding)){
            LT_error("Special form let binding must be pair");
        }

        binding_form_cursor = binding;
        LT_OBJECT_ARG(binding_form_cursor, symbol);
        LT_OBJECT_ARG(binding_form_cursor, value_expression);
        LT_ARG_END(binding_form_cursor);

        if (!LT_Symbol_p(symbol)){
            LT_error("Special form let binding name must be symbol");
        }

        value = LT_eval(value_expression, environment, NULL);
        LT_ListBuilder_append(symbols, symbol);
        LT_ListBuilder_append(values, value);

        binding_cursor = LT_cdr(binding_cursor);
    }

    let_environment = LT_Environment_new(environment, LT_NIL, LT_NIL);
    binding_cursor = LT_ListBuilder_value(symbols);
    cursor = LT_ListBuilder_value(values);

    while (binding_cursor != LT_NIL){
        LT_Environment_bind(
            let_environment,
            LT_car(binding_cursor),
            LT_car(cursor),
            0
        );
        binding_cursor = LT_cdr(binding_cursor);
        cursor = LT_cdr(cursor);
    }

    return LT_eval_sequence(body, let_environment, tail_call_unwind_marker);
}

static LT_Value special_form_letrec(
    LT_Value arguments,
    LT_Environment* environment,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
){
    LT_Value cursor = arguments;
    LT_Value bindings;
    LT_Value body;
    LT_Value binding_cursor;
    LT_ListBuilder* symbols = LT_ListBuilder_new();
    LT_ListBuilder* value_expressions = LT_ListBuilder_new();
    LT_Environment* let_environment;

    LT_OBJECT_ARG(cursor, bindings);
    LT_ARG_REST(cursor, body);
    if (body == LT_NIL){
        LT_error("Special form letrec expects body");
    }

    let_environment = LT_Environment_new(environment, LT_NIL, LT_NIL);
    binding_cursor = bindings;
    while (binding_cursor != LT_NIL){
        LT_Value binding;
        LT_Value binding_form_cursor;
        LT_Value symbol;
        LT_Value value_expression;

        if (!LT_Pair_p(binding_cursor)){
            LT_error("Special form letrec expects proper binding list");
        }

        binding = LT_car(binding_cursor);
        if (!LT_Pair_p(binding)){
            LT_error("Special form letrec binding must be pair");
        }

        binding_form_cursor = binding;
        LT_OBJECT_ARG(binding_form_cursor, symbol);
        LT_OBJECT_ARG(binding_form_cursor, value_expression);
        LT_ARG_END(binding_form_cursor);

        if (!LT_Symbol_p(symbol)){
            LT_error("Special form letrec binding name must be symbol");
        }

        LT_ListBuilder_append(symbols, symbol);
        LT_ListBuilder_append(value_expressions, value_expression);
        LT_Environment_bind(let_environment, symbol, LT_NIL, 0);

        binding_cursor = LT_cdr(binding_cursor);
    }

    binding_cursor = LT_ListBuilder_value(symbols);
    cursor = LT_ListBuilder_value(value_expressions);

    while (binding_cursor != LT_NIL){
        LT_Value value = LT_eval(LT_car(cursor), let_environment, NULL);

        if (!LT_Environment_set(let_environment, LT_car(binding_cursor), value)){
            LT_error("Special form letrec expected existing mutable binding");
        }

        binding_cursor = LT_cdr(binding_cursor);
        cursor = LT_cdr(cursor);
    }

    return LT_eval_sequence(body, let_environment, tail_call_unwind_marker);
}

static LT_Value special_form_define(LT_Value arguments,
                                    LT_Environment* environment,
                                    LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value symbol;
    LT_Value value_expression;
    LT_Value value;

    LT_OBJECT_ARG(cursor, symbol);
    LT_OBJECT_ARG(cursor, value_expression);
    (void)tail_call_unwind_marker;

    if (!LT_Symbol_p(symbol)){
        LT_error("Special form define expects symbol as first argument");
    }
    LT_ARG_END(cursor);

    value = LT_eval(value_expression, environment, NULL);
    LT_Environment_bind(environment, symbol, value, 0);
    return value;
}

static LT_Value special_form_define_constant(
    LT_Value arguments,
    LT_Environment* environment,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
){
    LT_Value cursor = arguments;
    LT_Value symbol;
    LT_Value value_expression;
    LT_Value value;

    LT_OBJECT_ARG(cursor, symbol);
    LT_OBJECT_ARG(cursor, value_expression);
    (void)tail_call_unwind_marker;

    if (!LT_Symbol_p(symbol)){
        LT_error("Special form define-constant expects symbol as first argument");
    }
    LT_ARG_END(cursor);

    value = LT_eval(value_expression, environment, NULL);
    LT_Environment_bind(environment, symbol, value, LT_ENV_BINDING_FLAG_CONSTANT);
    return value;
}

static LT_Value special_form_set_bang(LT_Value arguments,
                                      LT_Environment* environment,
                                      LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value symbol;
    LT_Value value_expression;
    LT_Value value;

    LT_OBJECT_ARG(cursor, symbol);
    LT_OBJECT_ARG(cursor, value_expression);
    (void)tail_call_unwind_marker;

    if (!LT_Symbol_p(symbol)){
        LT_error("Special form set! expects symbol as first argument");
    }
    LT_ARG_END(cursor);

    value = LT_eval(value_expression, environment, NULL);
    if (!LT_Environment_set(environment, symbol, value)){
        LT_error("Special form set! expected existing mutable binding");
    }
    return value;
}

static int LT_Macro_p_implementation(LT_Value value){
    return LT_Primitive_p(value)
        || LT_Closure_p(value);
}

static LT_Value special_form_macro(LT_Value arguments,
                                   LT_Environment* environment,
                                   LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value callable_expression;
    LT_Value callable;

    LT_OBJECT_ARG(cursor, callable_expression);
    LT_ARG_END(cursor);
    (void)tail_call_unwind_marker;

    callable = LT_eval(callable_expression, environment, NULL);
    if (!LT_Macro_p_implementation(callable)){
        LT_error("Special form macro expects primitive or closure");
    }

    return LT_Macro_new(callable);
}

static LT_Value special_form_throw(LT_Value arguments,
                                   LT_Environment* environment,
                                   LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value tag_expression;
    LT_Value value_expression;
    LT_Value tag;
    LT_Value value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, tag_expression);
    LT_OBJECT_ARG(cursor, value_expression);
    LT_ARG_END(cursor);

    tag = LT_eval(tag_expression, environment, NULL);
    value = LT_eval(value_expression, environment, NULL);
    LT_throw(tag, value);
    return LT_NIL;
}

static LT_Value special_form_catch(LT_Value arguments,
                                   LT_Environment* environment,
                                   LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value tag_expression;
    LT_Value body;
    LT_Value tag;
    LT_Value result = LT_NIL;

    LT_OBJECT_ARG(cursor, tag_expression);
    LT_ARG_REST(cursor, body);

    tag = LT_eval(tag_expression, environment, NULL);
    LT_CATCH(tag, result, {
        result = LT_eval_sequence(body, environment, tail_call_unwind_marker);
    });

    return result;
}

static LT_Value special_form_unwind_protect(
    LT_Value arguments,
    LT_Environment* environment,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
){
    LT_Value cursor = arguments;
    LT_Value protected_expression;
    LT_Value cleanup_body;
    LT_Value result = LT_NIL;

    LT_OBJECT_ARG(cursor, protected_expression);
    LT_ARG_REST(cursor, cleanup_body);
    (void)tail_call_unwind_marker;

    LT_UNWIND_PROTECT(
    {
        result = LT_eval(protected_expression, environment, NULL);
    },
    {
        (void)LT_eval_sequence(cleanup_body, environment, NULL);
    });

    return result;
}

static LT_Value special_form_handler_bind(
    LT_Value arguments,
    LT_Environment* environment,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
){
    LT_Value cursor = arguments;
    LT_Value handler_expression;
    LT_Value body;
    LT_Value handler;
    LT_Value result = LT_NIL;

    LT_OBJECT_ARG(cursor, handler_expression);
    LT_ARG_REST(cursor, body);

    handler = LT_eval(handler_expression, environment, NULL);
    LT_HANDLER_BIND(handler, {
        result = LT_eval_sequence(body, environment, tail_call_unwind_marker);
    });
    return result;
}

static LT_Value special_form_get_current_environment(
    LT_Value arguments,
    LT_Environment* environment,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
){
    LT_Value cursor = arguments;
    (void)tail_call_unwind_marker;
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)environment;
}

static LT_Value special_form_send(LT_Value arguments,
                                  LT_Environment* environment,
                                  LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value receiver_expression;
    LT_Value selector_expression;
    LT_Value message_argument_expressions;

    LT_OBJECT_ARG(cursor, receiver_expression);
    LT_OBJECT_ARG(cursor, selector_expression);
    LT_ARG_REST(cursor, message_argument_expressions);

    return LT_send(
        LT_eval(receiver_expression, environment, NULL),
        LT_eval(selector_expression, environment, NULL),
        LT_eval_argument_list(message_argument_expressions, environment),
        tail_call_unwind_marker
    );
}

static LT_Value special_form_super_send(
    LT_Value arguments,
    LT_Environment* environment,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
){
    LT_Value cursor = arguments;
    LT_Value receiver_expression;
    LT_Value selector_expression;
    LT_Value message_argument_expressions;
    LT_Value precedence_list;

    LT_OBJECT_ARG(cursor, receiver_expression);
    LT_OBJECT_ARG(cursor, selector_expression);
    LT_ARG_REST(cursor, message_argument_expressions);

    precedence_list = LT_Environment_invocation_context_of_kind(
        environment,
        (LT_Value)(uintptr_t)&LT_send_invocation_context
    );
    if (precedence_list == LT_NIL){
        LT_error("%super-send requires send invocation context");
    }

    return LT_super_send(
        LT_eval(receiver_expression, environment, NULL),
        precedence_list,
        LT_eval(selector_expression, environment, NULL),
        LT_eval_argument_list(message_argument_expressions, environment),
        tail_call_unwind_marker
    );
}


static LT_SpecialForm quote_special_form = {
    .function = special_form_quote,
    .expand_function = expand_special_form_quote,
    .name = "quote",
    .arguments = "(value)",
    .description = "Return value without evaluation."
};

static LT_SpecialForm quasiquote_special_form = {
    .function = special_form_quasiquote,
    .expand_function = expand_special_form_quote,
    .name = "quasiquote",
    .arguments = "(value)",
    .description = "Evaluate quasiquote template."
};

static LT_SpecialForm lambda_special_form = {
    .function = special_form_lambda,
    .expand_function = expand_special_form_lambda,
    .name = "%lambda",
    .arguments = "(name (arg ...) body ...)",
    .description = "Create closure with lexical scope."
};

static LT_SpecialForm if_special_form = {
    .function = special_form_if,
    .expand_function = expand_special_form_default,
    .name = "if",
    .arguments = "(condition then [else])",
    .description = "Evaluate then or else based on condition."
};

static LT_SpecialForm and_special_form = {
    .function = special_form_and,
    .expand_function = expand_special_form_default,
    .name = "and",
    .arguments = "(expression ...)",
    .description = "Evaluate expressions until one is falsey, else return last value."
};

static LT_SpecialForm or_special_form = {
    .function = special_form_or,
    .expand_function = expand_special_form_default,
    .name = "or",
    .arguments = "(expression ...)",
    .description = "Evaluate expressions until one is truthy, else return last value."
};

static LT_SpecialForm cond_special_form = {
    .function = special_form_cond,
    .expand_function = expand_special_form_default,
    .name = "cond",
    .arguments = "((test body ...) ...)",
    .description = "Evaluate the body of the first clause with a truthy test."
};

static LT_SpecialForm begin_special_form = {
    .function = special_form_begin,
    .expand_function = expand_special_form_default,
    .name = "begin",
    .arguments = "(body ...)",
    .description = "Evaluate body forms in order in the current environment."
};

static LT_SpecialForm let_special_form = {
    .function = special_form_let,
    .expand_function = expand_special_form_default,
    .name = "%let",
    .arguments = "(((symbol value-expression) ...) body ...)",
    .description = "Evaluate body in lexical scope with local bindings."
};

static LT_SpecialForm letrec_special_form = {
    .function = special_form_letrec,
    .expand_function = expand_special_form_default,
    .name = "letrec",
    .arguments = "(((symbol value-expression) ...) body ...)",
    .description = "Evaluate body in lexical scope with mutually recursive bindings."
};

static LT_SpecialForm define_special_form = {
    .function = special_form_define,
    .expand_function = expand_special_form_define,
    .name = "%define",
    .arguments = "(symbol value-expression)",
    .description = "Create mutable binding in current environment."
};

static LT_SpecialForm define_constant_special_form = {
    .function = special_form_define_constant,
    .expand_function = expand_special_form_define,
    .name = "%define-constant",
    .arguments = "(symbol value-expression)",
    .description = "Create constant binding in current environment."
};

static LT_SpecialForm set_bang_special_form = {
    .function = special_form_set_bang,
    .expand_function = expand_special_form_set_bang,
    .name = "%set!",
    .arguments = "(symbol value-expression)",
    .description = "Update existing mutable binding."
};

static LT_SpecialForm macro_special_form = {
    .function = special_form_macro,
    .expand_function = expand_special_form_default,
    .name = "macro",
    .arguments = "(callable-expression)",
    .description = "Wrap primitive or closure as macro."
};

static LT_SpecialForm throw_special_form = {
    .function = special_form_throw,
    .expand_function = expand_special_form_default,
    .name = "throw",
    .arguments = "(tag-expression value-expression)",
    .description = "Throw value to nearest enclosing catch with matching tag."
};

static LT_SpecialForm catch_special_form = {
    .function = special_form_catch,
    .expand_function = expand_special_form_default,
    .name = "catch",
    .arguments = "(tag-expression body ...)",
    .description = "Evaluate body and intercept throws matching tag."
};

static LT_SpecialForm unwind_protect_special_form = {
    .function = special_form_unwind_protect,
    .expand_function = expand_special_form_default,
    .name = "unwind-protect",
    .arguments = "(protected-expression cleanup ...)",
    .description = "Always run cleanup forms; rethrow non-local exits."
};

static LT_SpecialForm handler_bind_special_form = {
    .function = special_form_handler_bind,
    .expand_function = expand_special_form_default,
    .name = "handler-bind",
    .arguments = "(handler-expression body ...)",
    .description = "Bind condition handler during dynamic extent of body."
};

static LT_SpecialForm get_current_environment_special_form = {
    .function = special_form_get_current_environment,
    .expand_function = expand_special_form_default,
    .name = "get-current-environment",
    .arguments = "()",
    .description = "Return current lexical environment."
};

static LT_SpecialForm send_special_form = {
    .function = special_form_send,
    .expand_function = expand_special_form_default,
    .name = "%send",
    .arguments = "(receiver selector argument ...)",
    .description = "Evaluate and send selector to receiver."
};

static LT_SpecialForm super_send_special_form = {
    .function = special_form_super_send,
    .expand_function = expand_special_form_default,
    .name = "%super-send",
    .arguments = "(receiver selector argument ...)",
    .description = "Evaluate and send selector using send invocation context."
};

static void bind_static_special_form(LT_Environment* environment,
                                     LT_SpecialForm* special_form){
    LT_Value special_form_value = LT_SpecialForm_from_static(special_form);

    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(LT_PACKAGE_LISTTALK, special_form->name),
        special_form_value,
        LT_ENV_BINDING_FLAG_CONSTANT
    );
}

static void bind_static_special_form_in(LT_Environment* environment,
                                        LT_Package* package,
                                        LT_SpecialForm* special_form){
    LT_Value special_form_value = LT_SpecialForm_from_static(special_form);

    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(package, special_form->name),
        special_form_value,
        LT_ENV_BINDING_FLAG_CONSTANT
    );
}

void LT_base_env_bind_special_forms(LT_Environment* environment){
    bind_static_special_form(environment, &quote_special_form);
    bind_static_special_form(environment, &quasiquote_special_form);
    bind_static_special_form(environment, &if_special_form);
    bind_static_special_form(environment, &and_special_form);
    bind_static_special_form(environment, &or_special_form);
    bind_static_special_form(environment, &cond_special_form);
    bind_static_special_form(environment, &begin_special_form);
    bind_static_special_form(environment, &letrec_special_form);
    bind_static_special_form_in(
        environment,
        LT_PACKAGE_LISTTALK_IMPLEMENTATION,
        &let_special_form
    );
    bind_static_special_form_in(
        environment,
        LT_PACKAGE_LISTTALK_IMPLEMENTATION,
        &lambda_special_form
    );
    bind_static_special_form_in(
        environment,
        LT_PACKAGE_LISTTALK_IMPLEMENTATION,
        &define_special_form
    );
    bind_static_special_form_in(
        environment,
        LT_PACKAGE_LISTTALK_IMPLEMENTATION,
        &define_constant_special_form
    );
    bind_static_special_form_in(
        environment,
        LT_PACKAGE_LISTTALK_IMPLEMENTATION,
        &set_bang_special_form
    );
    bind_static_special_form(environment, &macro_special_form);
    bind_static_special_form(environment, &throw_special_form);
    bind_static_special_form(environment, &catch_special_form);
    bind_static_special_form(environment, &unwind_protect_special_form);
    bind_static_special_form(environment, &handler_bind_special_form);
    bind_static_special_form(environment, &get_current_environment_special_form);
    bind_static_special_form_in(
        environment,
        LT_PACKAGE_LISTTALK_IMPLEMENTATION,
        &send_special_form
    );
    bind_static_special_form_in(
        environment,
        LT_PACKAGE_LISTTALK_IMPLEMENTATION,
        &super_send_special_form
    );
}
