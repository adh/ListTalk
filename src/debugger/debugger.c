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

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    LT_Value restarts;
    LT_Environment* environment;
} DebuggerContext;

typedef struct {
    LT_Value object;
    LT_Value slots;
} InspectorContext;

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

static void debugger_repl_object(LT_Value object, void* opaque){
    DebuggerContext* context = opaque;
    LT_Value restart_value = debugger_restart_selected(object, context->restarts);

    if (restart_value != LT_INVALID){
        LT_Restart* restart = LT_Restart_from_value(restart_value);
        LT_apply(
            LT_Restart_callable(restart),
            LT_NIL,
            LT_NIL,
            LT_NIL,
            NULL
        );
        return;
    }

    if (LT_SmallInteger_p(object)
        || (LT_Symbol_p(object)
            && LT_Symbol_package(LT_Symbol_from_value(object))
                == LT_PACKAGE_KEYWORD)){
        fputs("No such restart.\n", stdout);
        return;
    }

    object = LT_eval(object, context->environment, NULL);
    LT_Value_debugPrintOn(object, stdout);
    fputc('\n', stdout);
}

void LT_Debugger_break(LT_Value condition){
    LT_REPL_State* repl = LT_REPL_State_new();
    DebuggerContext context = {
        .restarts = LT_current_restarts(),
        .environment = LT_new_base_environment()
    };

    fputs("Condition: ", stdout);
    LT_Value_debugPrintOn(condition, stdout);
    fputc('\n', stdout);
    if (LT_stack_trace_depth() == 0){
        fputs("Backtrace:\n  (empty)\n", stdout);
    } else {
        LT_print_backtrace(stdout);
    }
    debugger_print_restarts(context.restarts);

    LT_REPL_State_set_prompt(repl, "debug> ");
    LT_REPL_State_loop(repl, debugger_repl_object, &context);
}

static void inspector_print(InspectorContext* context){
    LT_Value cursor;
    unsigned int index = 1;

    fputs("Object: ", stdout);
    LT_Value_debugPrintOn(context->object, stdout);
    fputc('\n', stdout);

    context->slots = LT_Class_slots(LT_Value_class(context->object));
    cursor = context->slots;
    if (cursor == LT_NIL){
        fputs("Slots: (none)\n", stdout);
        return;
    }

    fputs("Slots:\n", stdout);
    while (LT_Pair_p(cursor)){
        LT_Value slot_name = LT_car(cursor);

        fprintf(stdout, "  %u: ", index++);
        LT_Value_debugPrintOn(slot_name, stdout);
        fputs(" = ", stdout);
        LT_Value_debugPrintOn(
            LT_Object_slot_ref(context->object, slot_name),
            stdout
        );
        fputc('\n', stdout);
        cursor = LT_cdr(cursor);
    }
}

static LT_Value inspector_selected_slot(LT_Value input, LT_Value slots){
    LT_Value cursor;

    if (LT_SmallInteger_p(input)){
        return list_at_1_based(slots, LT_SmallInteger_value(input));
    }
    if (!LT_Symbol_p(input)){
        return LT_INVALID;
    }

    cursor = slots;
    while (LT_Pair_p(cursor)){
        LT_Value slot_name = LT_car(cursor);

        if (LT_Symbol_p(slot_name)
            && strcmp(
                LT_Symbol_name(LT_Symbol_from_value(input)),
                LT_Symbol_name(LT_Symbol_from_value(slot_name))
            ) == 0){
            return slot_name;
        }
        cursor = LT_cdr(cursor);
    }
    return LT_INVALID;
}

static void inspector_repl_object(LT_Value input, void* opaque){
    InspectorContext* context = opaque;
    LT_Value slot_name = inspector_selected_slot(input, context->slots);

    if (slot_name == LT_INVALID){
        fputs("No such slot.\n", stdout);
        return;
    }
    context->object = LT_Object_slot_ref(context->object, slot_name);
    inspector_print(context);
}

void LT_Debugger_inspect(LT_Value object){
    LT_REPL_State* repl = LT_REPL_State_new();
    InspectorContext context = {.object = object, .slots = LT_NIL};

    inspector_print(&context);
    LT_REPL_State_set_prompt(repl, "inspect> ");
    LT_REPL_State_loop(repl, inspector_repl_object, &context);
}
