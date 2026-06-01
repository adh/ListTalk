/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>

#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Closure.h>
#include <ListTalk/classes/Message.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Macro.h>
#include <ListTalk/classes/SpecialForm.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/classes/Reader.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/stack_trace.h>

#include <ctype.h>
#include <setjmp.h>
#include <string.h>

struct LT_TailCallUnwindMarker_s {
    jmp_buf jump_buffer;
    LT_Value callable;
    LT_Value arguments;
    LT_Value invocation_context_kind;
    LT_Value invocation_context_data;
    LT_StackFrame* stack_trace_top;
};

static LT_Value eval_form(LT_Value expression,
                          LT_Environment* environment,
                          LT_TailCallUnwindMarker* tail_call_unwind_marker);
static LT_Value immutable_list_from_expansion(
    LT_Value expansion,
    LT_Value original_expression
);
static LT_Value send_does_not_understand(
    LT_Value receiver,
    LT_Value selector,
    LT_Value arguments,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
);
static LT_Value copy_message_arguments(LT_Value arguments);

static int keyword_marker_p(LT_Value value, const char* name){
    if (!LT_Symbol_p(value)){
        return 0;
    }
    if (LT_Symbol_package(LT_Symbol_from_value(value)) != LT_PACKAGE_KEYWORD){
        return 0;
    }
    return strcmp(LT_Symbol_name(LT_Symbol_from_value(value)), name) == 0;
}

static LT_Value keyword_for_parameter(LT_Value parameter){
    if (!LT_Symbol_p(parameter)){
        LT_error("Closure keyword parameter name must be symbol");
    }
    return LT_Symbol_new_in(
        LT_PACKAGE_KEYWORD,
        LT_Symbol_name(LT_Symbol_from_value(parameter))
    );
}

static LT_Value keyword_parameter_name(LT_Value parameter){
    if (LT_Symbol_p(parameter)){
        return parameter;
    }
    if (LT_Pair_p(parameter)){
        LT_Value name = LT_car(parameter);
        LT_Value tail = LT_cdr(parameter);

        if (!LT_Symbol_p(name)){
            LT_error("Closure keyword parameter name must be symbol");
        }
        if (!LT_Pair_p(tail) || LT_cdr(tail) != LT_NIL){
            LT_error("Closure keyword parameter default must be a two element list");
        }
        return name;
    }
    LT_error("Closure keyword parameter must be symbol or two element list");
    return LT_NIL;
}

static LT_Value keyword_parameter_default_expression(LT_Value parameter){
    if (LT_Symbol_p(parameter)){
        return LT_NIL;
    }
    return LT_car(LT_cdr(parameter));
}

static int keyword_argument_lookup(LT_Value arguments,
                                   LT_Value keyword,
                                   LT_Value* value_out){
    LT_Value cursor = arguments;

    while (cursor != LT_NIL){
        LT_Value supplied_keyword;
        LT_Value value_tail;

        if (!LT_Pair_p(cursor)){
            LT_error("Closure keyword arguments must be a proper list");
        }
        supplied_keyword = LT_car(cursor);
        if (!LT_Symbol_p(supplied_keyword)
            || LT_Symbol_package(LT_Symbol_from_value(supplied_keyword))
                != LT_PACKAGE_KEYWORD){
            LT_error("Closure keyword argument name must be keyword");
        }

        value_tail = LT_cdr(cursor);
        if (!LT_Pair_p(value_tail)){
            LT_error("Closure keyword argument missing value");
        }
        if (supplied_keyword == keyword){
            *value_out = LT_car(value_tail);
            return 1;
        }
        cursor = LT_cdr(value_tail);
    }

    return 0;
}

static void ensure_no_unknown_keyword_arguments(LT_Value arguments,
                                                LT_Value keyword_parameters){
    LT_Value argument_cursor = arguments;

    while (argument_cursor != LT_NIL){
        LT_Value supplied_keyword;
        LT_Value value_tail;
        LT_Value parameter_cursor = keyword_parameters;
        int found = 0;

        if (!LT_Pair_p(argument_cursor)){
            LT_error("Closure keyword arguments must be a proper list");
        }
        supplied_keyword = LT_car(argument_cursor);
        if (!LT_Symbol_p(supplied_keyword)
            || LT_Symbol_package(LT_Symbol_from_value(supplied_keyword))
                != LT_PACKAGE_KEYWORD){
            LT_error("Closure keyword argument name must be keyword");
        }
        value_tail = LT_cdr(argument_cursor);
        if (!LT_Pair_p(value_tail)){
            LT_error("Closure keyword argument missing value");
        }

        while (LT_Pair_p(parameter_cursor)){
            LT_Value parameter_name = keyword_parameter_name(
                LT_car(parameter_cursor)
            );

            if (supplied_keyword == keyword_for_parameter(parameter_name)){
                found = 1;
                break;
            }
            parameter_cursor = LT_cdr(parameter_cursor);
        }
        if (!found){
            LT_error("Unknown closure keyword argument");
        }

        argument_cursor = LT_cdr(value_tail);
    }
}

static LT_Value immutable_list_from_expansion(
    LT_Value expansion,
    LT_Value original_expression
){
    LT_Value cursor = expansion;
    LT_Value tail;
    LT_Value source_location = LT_NIL;
    LT_Value source_file = LT_NIL;
    LT_Value* values;
    size_t count = 0;
    size_t i;

    if (!LT_Pair_p(expansion)){
        return expansion;
    }

    if (LT_ImmutableList_p(expansion)){
        source_location = LT_ImmutableList_source_location(expansion);
        source_file = LT_ImmutableList_source_file(expansion);
        if (LT_ImmutableList_original_expression(expansion) == original_expression){
            return expansion;
        }
    } else if (original_expression == LT_NIL){
        return LT_ImmutableList_fromList(expansion);
    }

    while (LT_Pair_p(cursor)){
        count++;
        cursor = LT_cdr(cursor);
    }
    tail = cursor;

    values = GC_MALLOC(sizeof(LT_Value) * count);
    cursor = expansion;
    for (i = 0; i < count; i++){
        values[i] = LT_car(cursor);
        cursor = LT_cdr(cursor);
    }

    return LT_ImmutableList_new_with_trailer(
        count,
        values,
        tail,
        source_location,
        source_file,
        original_expression
    );
}

static LT_Value send_from_precedence(LT_Value receiver,
                                     LT_Value precedence_list,
                                     LT_Value selector,
                                     LT_Value arguments,
                                     LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = precedence_list;

    while (cursor != LT_NIL){
        LT_Value class_value = LT_ImmutableList_car(cursor);
        LT_Class* current = LT_Class_from_object(class_value);
        LT_IdentityDictionary* methods = LT_IdentityDictionary_from_value(
            current->methods
        );
        LT_Value method;

        if (LT_IdentityDictionary_at(methods, selector, &method)){
            return LT_apply(
                method,
                LT_cons(receiver, arguments),
                (LT_Value)(uintptr_t)&LT_send_invocation_context,
                LT_ImmutableList_cdr(cursor),
                tail_call_unwind_marker
            );
        }

        cursor = LT_ImmutableList_cdr(cursor);
    }

    return send_does_not_understand(
        receiver,
        selector,
        arguments,
        tail_call_unwind_marker
    );
}

static LT_Value send_does_not_understand(
    LT_Value receiver,
    LT_Value selector,
    LT_Value arguments,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
){
    LT_Value message_arguments = copy_message_arguments(arguments);
    LT_Value message = LT_Message_new(selector, receiver, message_arguments);
    LT_Value dnu_arguments = LT_cons(message, LT_NIL);

    return LT_send(
        receiver,
        LT_Symbol_new_in(LT_PACKAGE_KEYWORD, "doesNotUnderstand:"),
        dnu_arguments,
        tail_call_unwind_marker
    );
}

static LT_Value copy_message_arguments(LT_Value arguments){
    LT_Value cursor = arguments;
    LT_Value tail;
    LT_Value* values;
    size_t count = 0;
    size_t i;

    if (arguments == LT_NIL){
        return LT_NIL;
    }
    if (!LT_Pair_p(arguments)){
        LT_type_error(arguments, &LT_List_class);
    }

    while (LT_Pair_p(cursor)){
        count++;
        cursor = LT_cdr(cursor);
    }
    tail = cursor;

    values = GC_MALLOC(sizeof(LT_Value) * count);
    cursor = arguments;
    for (i = 0; i < count; i++){
        values[i] = LT_car(cursor);
        cursor = LT_cdr(cursor);
    }

    return LT_ImmutableList_new_with_tail(count, values, tail);
}

static int stream_has_next_form(LT_ReaderStream* stream){
    int ch = LT_ReaderStream_getc(stream);

    while (1){
        while (ch != EOF && isspace((unsigned char)ch)){
            ch = LT_ReaderStream_getc(stream);
        }

        if (ch == ';'){
            while (ch != EOF && ch != '\n'){
                ch = LT_ReaderStream_getc(stream);
            }
            ch = LT_ReaderStream_getc(stream);
            continue;
        }

        if (ch == EOF){
            return 0;
        }

        LT_ReaderStream_ungetc(stream, ch);
        return 1;
    }
}

LT_Value LT_eval_argument_list(LT_Value list, LT_Environment* environment){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    LT_Value cursor = list;

    while (cursor != LT_NIL){
        if (!LT_Pair_p(cursor)){
            LT_error("Application expects a proper list of arguments");
        }
        LT_ListBuilder_append(
            builder,
            eval_form(LT_car(cursor), environment, NULL)
        );
        cursor = LT_cdr(cursor);
    }

    return LT_ListBuilder_value(builder);
}

static void bind_closure_parameters(LT_Value parameters,
                                    LT_Value arguments,
                                    LT_Environment* target_environment){
    LT_Value parameter_cursor = parameters;
    LT_Value argument_cursor = arguments;

    if (LT_Symbol_p(parameter_cursor)){
        LT_Environment_bind(
            target_environment,
            parameter_cursor,
            argument_cursor,
            0
        );
        return;
    }

    while (LT_Pair_p(parameter_cursor)){
        LT_Value parameter;

        parameter = LT_car(parameter_cursor);
        if (keyword_marker_p(parameter, "key")){
            LT_Value keyword_parameters = LT_cdr(parameter_cursor);
            LT_Value keyword_parameter_cursor = keyword_parameters;

            ensure_no_unknown_keyword_arguments(
                argument_cursor,
                keyword_parameters
            );

            while (keyword_parameter_cursor != LT_NIL){
                LT_Value keyword_parameter;
                LT_Value parameter_name;
                LT_Value keyword;
                LT_Value value;

                if (!LT_Pair_p(keyword_parameter_cursor)){
                    LT_error("Closure keyword parameter list must be proper");
                }

                keyword_parameter = LT_car(keyword_parameter_cursor);
                parameter_name = keyword_parameter_name(keyword_parameter);
                keyword = keyword_for_parameter(parameter_name);

                if (!keyword_argument_lookup(argument_cursor, keyword, &value)){
                    value = eval_form(
                        keyword_parameter_default_expression(keyword_parameter),
                        target_environment,
                        NULL
                    );
                }

                LT_Environment_bind(
                    target_environment,
                    parameter_name,
                    value,
                    0
                );

                keyword_parameter_cursor = LT_cdr(keyword_parameter_cursor);
            }
            return;
        }

        if (!LT_Pair_p(argument_cursor)){
            LT_error("Closure arity mismatch");
        }

        if (!LT_Symbol_p(parameter)){
            LT_error("Closure parameter must be symbol");
        }

        LT_Environment_bind(
            target_environment,
            parameter,
            LT_car(argument_cursor),
            0
        );

        parameter_cursor = LT_cdr(parameter_cursor);
        argument_cursor = LT_cdr(argument_cursor);
    }

    if (LT_Symbol_p(parameter_cursor)){
        LT_Environment_bind(
            target_environment,
            parameter_cursor,
            argument_cursor,
            0
        );
        return;
    }

    if (parameter_cursor != LT_NIL){
        LT_error("Closure parameter list must be proper or dotted with symbol");
    }
    if (argument_cursor != LT_NIL){
        LT_error("Closure arity mismatch");
    }
}

LT_Value LT_eval_sequence(LT_Value body,
                          LT_Environment* environment,
                          LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = body;
    LT_Value result = LT_NIL;

    while (cursor != LT_NIL){
        LT_Value next_cursor;

        if (!LT_Pair_p(cursor)){
            LT_error("Closure body expects proper list of forms");
        }
        next_cursor = LT_cdr(cursor);
        result = eval_form(
            LT_car(cursor),
            environment,
            (next_cursor == LT_NIL) ? tail_call_unwind_marker : NULL
        );
        cursor = next_cursor;
    }

    return result;
}

static LT_Value apply_closure(LT_Value closure_value,
                              LT_Value evaluated_arguments,
                              LT_Value invocation_context_kind,
                              LT_Value invocation_context_data,
                              LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Closure* closure = LT_Closure_from_value(closure_value);
    LT_Environment* application_environment = LT_Environment_new(
        LT_Closure_environment(closure),
        invocation_context_kind,
        invocation_context_data
    );

    bind_closure_parameters(
        LT_Closure_parameters(closure),
        evaluated_arguments,
        application_environment
    );

    return LT_eval_sequence(
        LT_Closure_body(closure),
        application_environment,
        tail_call_unwind_marker
    );
}

LT_Value LT_apply(LT_Value callable,
                  LT_Value arguments,
                  LT_Value invocation_context_kind,
                  LT_Value invocation_context_data,
                  LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_TailCallUnwindMarker local_tail_call_unwind_marker;
    LT_StackFrame stack_frame;
    int jump_value;

    if (tail_call_unwind_marker != NULL){
        tail_call_unwind_marker->callable = callable;
        tail_call_unwind_marker->arguments = arguments;
        tail_call_unwind_marker->invocation_context_kind = invocation_context_kind;
        tail_call_unwind_marker->invocation_context_data = invocation_context_data;
        LT_stack_trace_restore(tail_call_unwind_marker->stack_trace_top);
        longjmp(tail_call_unwind_marker->jump_buffer, 1);
    }

    stack_frame.type = LT_STACK_FRAME_TYPE_APPLY;
    stack_frame.arguments.apply.callable = callable;
    stack_frame.arguments.apply.arguments = arguments;
    LT_stack_trace_push(&stack_frame);

    local_tail_call_unwind_marker.callable = callable;
    local_tail_call_unwind_marker.arguments = arguments;
    local_tail_call_unwind_marker.invocation_context_kind = invocation_context_kind;
    local_tail_call_unwind_marker.invocation_context_data = invocation_context_data;
    local_tail_call_unwind_marker.stack_trace_top = &stack_frame;

    while (1){
        jump_value = setjmp(local_tail_call_unwind_marker.jump_buffer);
        (void)jump_value;

        callable = local_tail_call_unwind_marker.callable;
        arguments = local_tail_call_unwind_marker.arguments;
        invocation_context_kind =
            local_tail_call_unwind_marker.invocation_context_kind;
        invocation_context_data =
            local_tail_call_unwind_marker.invocation_context_data;
        stack_frame.arguments.apply.callable = callable;
        stack_frame.arguments.apply.arguments = arguments;

        if (LT_Primitive_p(callable)){
            LT_Value result = LT_Primitive_call(
                callable,
                arguments,
                invocation_context_kind,
                invocation_context_data,
                &local_tail_call_unwind_marker
            );
            LT_stack_trace_pop(&stack_frame);
            return result;
        }
        if (LT_Closure_p(callable)){
            LT_Value result = apply_closure(
                callable,
                arguments,
                invocation_context_kind,
                invocation_context_data,
                &local_tail_call_unwind_marker
            );
            LT_stack_trace_pop(&stack_frame);
            return result;
        }

        LT_error("LT_apply expects primitive or closure callable");
    }

    return LT_NIL;
}

LT_Value LT_send(LT_Value receiver,
                 LT_Value selector,
                 LT_Value arguments,
                 LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Class* receiver_class = LT_Value_class(receiver);
    LT_Value next_precedence = LT_NIL;
    LT_Value method = LT_Class_lookup_method_with_next(
        receiver_class,
        selector,
        &next_precedence
    );

    if (method == LT_INVALID){
        return send_does_not_understand(
            receiver,
            selector,
            arguments,
            tail_call_unwind_marker
        );
    }

    return LT_apply(
        method,
        LT_cons(receiver, arguments),
        (LT_Value)(uintptr_t)&LT_send_invocation_context,
        next_precedence,
        tail_call_unwind_marker
    );
}

LT_Value LT_super_send(LT_Value receiver,
                       LT_Value precedence_list,
                       LT_Value selector,
                       LT_Value arguments,
                       LT_TailCallUnwindMarker* tail_call_unwind_marker){
    if (precedence_list != LT_NIL && !LT_ImmutableList_p(precedence_list)){
        LT_type_error(precedence_list, &LT_ImmutableList_class);
    }

    return send_from_precedence(
        receiver,
        precedence_list,
        selector,
        arguments,
        tail_call_unwind_marker
    );
}

static LT_Value apply_form(LT_Value expression,
                           LT_Environment* environment,
                           LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value operator = LT_car(expression);
    LT_Value argument_expressions = LT_cdr(expression);
    LT_Value evaluated_operator = eval_form(operator, environment, NULL);
    LT_Value expansion;
    LT_Value implementation;

    if (LT_Macro_p(evaluated_operator)){
        implementation = LT_Macro_callable(
            LT_Macro_from_value(evaluated_operator)
        );
        expansion = LT_apply(
            implementation,
            argument_expressions,
            LT_NIL,
            LT_NIL,
            NULL
        );
        expansion = immutable_list_from_expansion(expansion, expression);
        return LT_eval(expansion, environment, tail_call_unwind_marker);
    }

    if (LT_SpecialForm_p(evaluated_operator)){
        return LT_SpecialForm_apply(
            evaluated_operator,
            argument_expressions,
            environment,
            tail_call_unwind_marker
        );
    }

    return LT_apply(
        evaluated_operator,
        LT_eval_argument_list(argument_expressions, environment),
        LT_NIL,
        LT_NIL,
        tail_call_unwind_marker
    );
}

static LT_Value eval_symbol(LT_Value symbol, LT_Environment* environment){
    LT_Value value;

    if (!LT_Environment_lookup(environment, symbol, &value, NULL)){
        if (LT_Symbol_package(LT_Symbol_from_value(symbol))
            == LT_PACKAGE_KEYWORD){
            return symbol;
        }
        LT_error("Unbound symbol");
    }
    return value;
}

static LT_Value eval_form(LT_Value expression,
                          LT_Environment* environment,
                          LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_StackFrame stack_frame;

    stack_frame.type = LT_STACK_FRAME_TYPE_EVAL;
    stack_frame.arguments.eval.expression = expression;
    stack_frame.arguments.eval.environment = environment;
    LT_stack_trace_push(&stack_frame);

    if (LT_Symbol_p(expression)){
        LT_Value result = eval_symbol(expression, environment);
        LT_stack_trace_pop(&stack_frame);
        return result;
    }

    if (LT_Pair_p(expression)){
        LT_Value result = apply_form(
            expression,
            environment,
            tail_call_unwind_marker
        );
        LT_stack_trace_pop(&stack_frame);
        return result;
    }

    LT_stack_trace_pop(&stack_frame);
    return expression;
}

LT_Value LT_eval(LT_Value expression,
                 LT_Environment* environment,
                 LT_TailCallUnwindMarker* tail_call_unwind_marker){
    if (environment == NULL){
        LT_error("Evaluator expects environment");
    }

    return eval_form(expression, environment, tail_call_unwind_marker);
}

LT_Value LT_eval_sequence_string(const char* source, LT_Environment* environment){
    LT_Reader* reader;
    LT_ReaderStream* stream;
    LT_Value result = LT_NIL;

    if (source == NULL){
        LT_error("Evaluator expects source string");
    }
    if (environment == NULL){
        LT_error("Evaluator expects environment");
    }

    reader = LT_Reader_new(LT_NIL);
    stream = LT_ReaderStream_newForString(source);

    while (stream_has_next_form(stream)){
        result = LT_eval(LT_Reader_readObject(reader, stream), environment, NULL);
    }

    return result;
}
