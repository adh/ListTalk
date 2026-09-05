/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/debugger/debugger.h>

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Class.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Restart.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/repl/repl.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/stack_trace.h>
#include <ListTalk/vm/throw_catch.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    LT_Value condition;
    LT_Value backtrace;
    LT_Value restarts;
    LT_Environment* environment;
    LT_Value debugger_hook;
    LT_REPL_State* repl;
    unsigned int level;
} DebuggerContext;

typedef struct {
    LT_Value object;
    LT_Value slots;
    LT_Value contents;
} InspectorContext;

static _Thread_local unsigned int debugger_level = 0;

static LT_Value return_to_debugger_tag = LT_NIL;
static pthread_once_t return_to_debugger_tag_once = PTHREAD_ONCE_INIT;

static void return_to_debugger_tag_init(void){
    return_to_debugger_tag = LT_Symbol_new_uninterned("return-to-debugger");
}

static LT_Value return_to_debugger_tag_value(void){
    pthread_once(&return_to_debugger_tag_once, return_to_debugger_tag_init);
    return return_to_debugger_tag;
}

LT_DEFINE_PRIMITIVE(
    debugger_hook_primitive,
    "debugger-hook",
    "(condition debugger-hook)",
    "Enter the interactive debugger for an unhandled condition."
){
    LT_Value cursor = arguments;
    LT_Value condition;
    LT_Value debugger_hook;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, condition);
    LT_OBJECT_ARG(cursor, debugger_hook);
    LT_ARG_END(cursor);

    LT_Debugger_break(condition, debugger_hook);
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE_RESTART(
    return_to_debugger_restart,
    "return-to-debugger",
    "()",
    "Abort the current debugger evaluation and return to its REPL."
){
    LT_Value cursor = arguments;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_ARG_END(cursor);
    LT_throw(return_to_debugger_tag_value(), LT_TRUE);
}

LT_DEFINE_PRIMITIVE(
    inspect_primitive,
    "ListTalk:inspect",
    "(object)",
    "Interactively inspect an object."
){
    LT_Value cursor = arguments;
    LT_Value object;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, object);
    LT_ARG_END(cursor);
    LT_Debugger_inspect(object);
    return object;
}

void LT_Debugger_define_inspect(LT_Environment* environment){
    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "inspect"),
        LT_Primitive_from_static(&inspect_primitive),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
}

LT_Value LT_Debugger_get_hook(void){
    return LT_Primitive_from_static(&debugger_hook_primitive);
}

void LT_Debugger_enable(void){
    LT_set_debugger_hook(LT_Debugger_get_hook());
}

static const char* debugger_name_cstr(LT_Value value){
    if (LT_Symbol_p(value)){
        return LT_Symbol_name(LT_Symbol_from_value(value));
    }
    if (LT_String_p(value)){
        return LT_String_value_cstr(LT_String_from_value(value));
    }
    return NULL;
}

static LT_Value list_at_1_based(LT_Value list, int64_t index){
    int64_t current = 1;

    while (LT_Pair_p(list)){
        if (current == index){
            return LT_car(list);
        }
        current++;
        list = LT_cdr(list);
    }
    return LT_INVALID;
}

static void debugger_print_restarts(LT_Value restarts){
    LT_Value cursor = restarts;
    unsigned int index = 1;

    fputs("Restarts:\n", stdout);
    if (cursor == LT_NIL){
        fputs("  (none)\n", stdout);
        return;
    }
    while (LT_Pair_p(cursor)){
        LT_Restart* restart = LT_Restart_from_value(LT_car(cursor));
        LT_Value name = LT_Restart_name(restart);
        LT_Value description = LT_Restart_description(restart);

        fprintf(stdout, "  %u: ", index++);
        LT_Value_debugPrintOn(name, stdout);
        if (description != LT_NIL){
            fputs(" -- ", stdout);
            if (LT_String_p(description)){
                fputs(
                    LT_String_value_cstr(LT_String_from_value(description)),
                    stdout
                );
            } else {
                LT_Value_debugPrintOn(description, stdout);
            }
        }
        fputc('\n', stdout);
        cursor = LT_cdr(cursor);
    }
}

static void debugger_print_condition_argument_name(LT_Value name){
    if (LT_Symbol_p(name)){
        fputs(LT_Symbol_name(LT_Symbol_from_value(name)), stdout);
    } else if (LT_String_p(name)){
        fputs(LT_String_value_cstr(LT_String_from_value(name)), stdout);
    } else {
        LT_Value_debugPrintOn(name, stdout);
    }
}

static void debugger_print_entered_on(LT_Value object){
    fputs("Debugger entered on:\n", stdout);
    if (!LT_Value_is_instance_of(
            object,
            (LT_Value)(uintptr_t)&LT_Condition_class
        )){
        fputs("  ", stdout);
        LT_Value_debugPrintOn(object, stdout);
        fputc('\n', stdout);
        return;
    }

    {
        LT_Value message = LT_Object_slot_ref(
            object,
            LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "message")
        );
        LT_Value arguments = LT_Object_slot_ref(
            object,
            LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "args")
        );

        fputs("  ", stdout);
        if (LT_String_p(message)){
            fputs(LT_String_value_cstr(LT_String_from_value(message)), stdout);
        } else {
            LT_Value_debugPrintOn(message, stdout);
        }
        fputc('\n', stdout);

        while (LT_Pair_p(arguments)){
            LT_Value name = LT_car(arguments);

            arguments = LT_cdr(arguments);
            if (!LT_Pair_p(arguments)){
                break;
            }
            fputs("    ", stdout);
            debugger_print_condition_argument_name(name);
            fputs(": ", stdout);
            LT_Value_debugPrintOn(LT_car(arguments), stdout);
            fputc('\n', stdout);
            arguments = LT_cdr(arguments);
        }
    }
}

static void debugger_set_prompt(DebuggerContext* context){
    LT_REPL_State_set_prompt(
        context->repl,
        LT_sprintf("debug[%u]> ", context->level)
    );
}

static void debugger_print_banner(DebuggerContext* context){
    debugger_print_entered_on(context->condition);
    fputc('\n', stdout);
    if (LT_stack_trace_depth() <= 1){
        fputs("Backtrace:\n  (empty)\n", stdout);
    } else {
        LT_stack_trace_print_skipping(stdout, 1);
    }
    fputc('\n', stdout);
    debugger_print_restarts(context->restarts);
}

static LT_Value debugger_restart_selected(LT_Value input, LT_Value restarts){
    LT_Value cursor;

    if (LT_SmallInteger_p(input)){
        return list_at_1_based(restarts, LT_SmallInteger_value(input));
    }
    if (!LT_Symbol_p(input)
        || LT_Symbol_package(LT_Symbol_from_value(input)) != LT_PACKAGE_KEYWORD){
        return LT_INVALID;
    }

    cursor = restarts;
    while (LT_Pair_p(cursor)){
        LT_Value restart_value = LT_car(cursor);
        const char* name = debugger_name_cstr(
            LT_Restart_name(LT_Restart_from_value(restart_value))
        );

        if (name != NULL
            && strcmp(name, LT_Symbol_name(LT_Symbol_from_value(input))) == 0){
            return restart_value;
        }
        cursor = LT_cdr(cursor);
    }
    return LT_INVALID;
}

static int debugger_restart_shorthand_p(LT_Value input){
    return LT_Pair_p(input)
        && LT_Symbol_p(LT_car(input))
        && LT_Symbol_package(LT_Symbol_from_value(LT_car(input)))
            == LT_PACKAGE_KEYWORD;
}

static LT_Value debugger_prompt_restart_arguments(
    DebuggerContext* context,
    LT_Restart* restart
){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    LT_Value cursor = LT_Restart_argument_list(restart);

    while (LT_Pair_p(cursor)){
        LT_String* printed_argument = LT_Value_asString(LT_car(cursor));
        LT_Value argument;

        LT_REPL_State_set_prompt(
            context->repl,
            LT_sprintf(
                "debug[%u] %s> ",
                context->level,
                LT_String_value_cstr(printed_argument)
            )
        );
        argument = LT_REPL_State_read(context->repl);
        if (argument == LT_INVALID){
            debugger_set_prompt(context);
            return LT_INVALID;
        }
        LT_ListBuilder_append(builder, argument);
        cursor = LT_cdr(cursor);
    }
    debugger_set_prompt(context);
    return LT_ListBuilder_value(builder);
}

static LT_Value debugger_eval_argument_list(
    LT_Value expressions,
    LT_Environment* environment
){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    LT_Value cursor = expressions;

    while (cursor != LT_NIL){
        if (!LT_Pair_p(cursor)){
            LT_error("Debugger restart shorthand expects proper argument list");
        }
        LT_ListBuilder_append(builder, LT_eval(LT_car(cursor), environment, NULL));
        cursor = LT_cdr(cursor);
    }
    return LT_ListBuilder_value(builder);
}

static int debugger_eval_restart_arguments(
    DebuggerContext* context,
    LT_Value expressions,
    LT_Value* arguments_out
){
    LT_Value returned_to_debugger = LT_NIL;

    LT_CATCH(return_to_debugger_tag_value(), returned_to_debugger, {
        LT_RESTART_BIND(LT_Restart_from_static(&return_to_debugger_restart), {
            LT_WITH_DEBUGGER_HOOK(context->debugger_hook, {
                *arguments_out = debugger_eval_argument_list(
                    expressions,
                    context->environment
                );
            });
        });
    });
    if (returned_to_debugger != LT_NIL){
        debugger_print_banner(context);
        debugger_set_prompt(context);
    }
    return returned_to_debugger == LT_NIL;
}

static int debugger_keyword_selected(LT_Value input, const char* name){
    return LT_Symbol_p(input)
        && LT_Symbol_package(LT_Symbol_from_value(input)) == LT_PACKAGE_KEYWORD
        && strcmp(
            LT_Symbol_name(LT_Symbol_from_value(input)),
            name
        ) == 0;
}

static void debugger_repl_object(LT_Value object, void* opaque){
    DebuggerContext* context = opaque;
    int restart_shorthand = debugger_restart_shorthand_p(object);
    LT_Value restart_designator = restart_shorthand ? LT_car(object) : object;
    LT_Value restart_value = debugger_restart_selected(
        restart_designator,
        context->restarts
    );
    LT_Value returned_to_debugger = LT_NIL;

    if (debugger_keyword_selected(object, "inspect-condition")){
        LT_Debugger_inspect(context->condition);
        return;
    }
    if (debugger_keyword_selected(object, "inspect-backtrace")){
        LT_Debugger_inspect(context->backtrace);
        return;
    }
    if (debugger_keyword_selected(object, "show")){
        debugger_print_banner(context);
        return;
    }

    if (restart_value != LT_INVALID){
        LT_Restart* restart = LT_Restart_from_value(restart_value);
        LT_Value restart_arguments = LT_NIL;

        if (restart_shorthand){
            if (!debugger_eval_restart_arguments(
                context,
                LT_cdr(object),
                &restart_arguments
            )){
                return;
            }
        } else if (LT_Restart_argument_list(restart) != LT_NIL){
            restart_arguments = debugger_prompt_restart_arguments(
                context,
                restart
            );
            if (restart_arguments == LT_INVALID){
                return;
            }
        }
        LT_apply(
            LT_Restart_callable(restart),
            restart_arguments,
            LT_NIL,
            LT_NIL,
            NULL
        );
        return;
    }

    if (restart_shorthand
        || LT_SmallInteger_p(object)
        || (LT_Symbol_p(object)
            && LT_Symbol_package(LT_Symbol_from_value(object))
                == LT_PACKAGE_KEYWORD)){
        fputs("No such restart.\n", stdout);
        return;
    }

    LT_CATCH(return_to_debugger_tag_value(), returned_to_debugger, {
        LT_RESTART_BIND(LT_Restart_from_static(&return_to_debugger_restart), {
            LT_WITH_DEBUGGER_HOOK(context->debugger_hook, {
                object = LT_eval(object, context->environment, NULL);
            });
        });
    });
    if (returned_to_debugger != LT_NIL){
        debugger_print_banner(context);
        debugger_set_prompt(context);
        return;
    }
    LT_Value_debugPrintOn(object, stdout);
    fputc('\n', stdout);
}

void LT_Debugger_break(LT_Value condition, LT_Value debugger_hook){
    LT_REPL_State* repl = LT_REPL_State_new();
    LT_Value backtrace = LT_stack_trace_capture();
    DebuggerContext context = {
        .condition = condition,
        .backtrace = LT_Pair_p(backtrace) ? LT_cdr(backtrace) : LT_NIL,
        .restarts = LT_current_restarts(),
        .environment = LT_new_base_environment(),
        .debugger_hook = debugger_hook,
        .repl = repl,
        .level = debugger_level + 1
    };

    LT_Environment_bind(
        context.environment,
        LT_Symbol_new_in(LT_PACKAGE_LISTTALK_DEBUG, "condition"),
        condition,
        LT_ENV_BINDING_FLAG_CONSTANT
    );
    LT_Environment_bind(
        context.environment,
        LT_Symbol_new_in(LT_PACKAGE_LISTTALK_DEBUG, "inspect"),
        LT_Primitive_from_static(&inspect_primitive),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
    LT_Environment_bind(
        context.environment,
        LT_Symbol_new_in(LT_PACKAGE_LISTTALK_DEBUG, "backtrace"),
        context.backtrace,
        LT_ENV_BINDING_FLAG_CONSTANT
    );

    debugger_level++;
    LT_UNWIND_PROTECT({
        debugger_print_banner(&context);
        debugger_set_prompt(&context);
        LT_WITH_PACKAGE(LT_PACKAGE_LISTTALK_DEBUG, {
            LT_REPL_State_loop(repl, debugger_repl_object, &context);
        });
    }, {
        debugger_level--;
    });
}

static void inspector_print_plist(const char* label,
                                  LT_Value plist){
    LT_Value cursor = plist;

    if (cursor == LT_NIL){
        fprintf(stdout, "%s (none)\n\n", label);
        return;
    }
    fprintf(stdout, "%s\n", label);
    while (cursor != LT_NIL){
        LT_Value key = LT_car(cursor);
        LT_Value value = LT_car(LT_cdr(cursor));

        fputs("  ", stdout);
        LT_Value_debugPrintOn(key, stdout);
        fputs(" = ", stdout);
        LT_Value_debugPrintOn(value, stdout);
        fputc('\n', stdout);
        cursor = LT_cdr(LT_cdr(cursor));
    }
    fputc('\n', stdout);
}

static void inspector_print(InspectorContext* context){
    LT_Value inspection_value = LT_SEND(context->object, "inspection");
    LT_ObjectInspection* inspection = LT_ObjectInspection_from_value(
        inspection_value
    );
    context->slots = LT_ObjectInspection_slots(inspection);
    context->contents = LT_ObjectInspection_contents(inspection);

    fputs("Object: ", stdout);
    fputs(
        LT_String_value_cstr(
            LT_String_from_value(LT_ObjectInspection_name(inspection))
        ),
        stdout
    );
    fputc('\n', stdout);
    fputs(
        LT_String_value_cstr(
            LT_String_from_value(LT_ObjectInspection_description(inspection))
        ),
        stdout
    );
    fputs("\n\n", stdout);

    inspector_print_plist("Slots:", context->slots);
    if (context->contents != LT_NIL){
        inspector_print_plist(
            LT_String_value_cstr(
                LT_String_from_value(
                    LT_ObjectInspection_contents_label(inspection)
                )
            ),
            context->contents
        );
    }
}

static LT_Value inspector_selected_from_plist(LT_Value input,
                                              LT_Value plist){
    LT_Value cursor;

    cursor = plist;
    while (cursor != LT_NIL){
        LT_Value key = LT_car(cursor);
        LT_Value value = LT_car(LT_cdr(cursor));

        if (LT_Value_equal_p(input, key)
            || (LT_Symbol_p(input)
            && LT_Symbol_p(key)
            && strcmp(
                LT_Symbol_name(LT_Symbol_from_value(input)),
                LT_Symbol_name(LT_Symbol_from_value(key))
            ) == 0)){
            return value;
        }
        cursor = LT_cdr(LT_cdr(cursor));
    }
    return LT_INVALID;
}

static LT_Value inspector_selected_entry(LT_Value input,
                                         LT_Value slots,
                                         LT_Value contents){
    LT_Value selected;

    selected = inspector_selected_from_plist(input, slots);
    if (selected != LT_INVALID){
        return selected;
    }
    return inspector_selected_from_plist(input, contents);
}

static void inspector_repl_object(LT_Value input, void* opaque){
    InspectorContext* context = opaque;
    LT_Value selected_object = inspector_selected_entry(
        input,
        context->slots,
        context->contents
    );

    if (selected_object == LT_INVALID){
        fputs("No such entry.\n", stdout);
        return;
    }
    LT_Debugger_inspect(selected_object);
    inspector_print(context);
}

void LT_Debugger_inspect(LT_Value object){
    LT_REPL_State* repl = LT_REPL_State_new();
    InspectorContext context = {
        .object = object,
        .slots = LT_NIL,
        .contents = LT_NIL
    };

    inspector_print(&context);
    LT_REPL_State_set_prompt(repl, "inspect> ");
    LT_REPL_State_loop(repl, inspector_repl_object, &context);
}
