/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Printer.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Package.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/classes/Closure.h>
#include <ListTalk/classes/Macro.h>
#include <ListTalk/classes/SpecialForm.h>
#include <ListTalk/classes/Object.h>
#include <ListTalk/classes/ImmutableList.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/Environment.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/compiler.h>
#include <ListTalk/vm/error.h>

#include <stdio.h>
#include <stdlib.h>

static int class_object_p(LT_Value value){
    return LT_Value_is_instance_of(value, LT_STATIC_CLASS(LT_Class));
}

static const char* name_from_symbol_or_string(LT_Value designator){
    if (LT_Symbol_p(designator)){
        return LT_Symbol_name(LT_Symbol_from_value(designator));
    }
    if (LT_String_p(designator)){
        return LT_String_value_cstr(LT_String_from_value(designator));
    }
    return NULL;
}

static LT_Package* package_from_designator(LT_Value designator, int create_missing){
    const char* package_name = NULL;
    LT_Package* package;

    if (LT_Package_p(designator)){
        return LT_Package_from_value(designator);
    }
    package_name = name_from_symbol_or_string(designator);
    if (package_name == NULL){
        LT_error("Package designator must be package, symbol, or string");
    }

    if (create_missing){
        return LT_Package_new((char*)package_name);
    }

    package = LT_Package_find((char*)package_name);
    if (package == NULL){
        LT_error("Referenced package does not exist");
    }
    return package;
}

static const char* package_nickname_from_designator(LT_Value designator){
    const char* name = name_from_symbol_or_string(designator);
    if (name == NULL){
        LT_error("Package nickname must be symbol or string");
    }
    return name;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_numeric_equal,
    "=",
    "(n n ...)",
    "Return true when all numeric arguments are equal.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value first;

    LT_OBJECT_ARG(cursor, first);
    if (!LT_Number_value_p(first)){
        LT_type_error(first, &LT_Number_class);
    }
    if (cursor == LT_NIL){
        LT_error("= requires at least two arguments");
    }

    while (cursor != LT_NIL){
        LT_Value next;
        LT_OBJECT_ARG(cursor, next);

        if (!LT_Number_value_p(next)){
            LT_type_error(next, &LT_Number_class);
        }
        if (!LT_Number_equal_p(first, next)){
            return LT_FALSE;
        }
        first = next;
    }

    return LT_TRUE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_eq_p,
    "eq?",
    "(left right ...)",
    "Return true when all arguments are the same value identity.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value first;

    LT_OBJECT_ARG(cursor, first);
    if (cursor == LT_NIL){
        LT_error("eq? requires at least two arguments");
    }

    while (cursor != LT_NIL){
        LT_Value next;
        LT_OBJECT_ARG(cursor, next);

        if (first != next){
            return LT_FALSE;
        }
    }

    return LT_TRUE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_eqv_p,
    "eqv?",
    "(left right ...)",
    "Return true when all arguments are numerically equivalent or identical.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value first;

    LT_OBJECT_ARG(cursor, first);
    if (cursor == LT_NIL){
        LT_error("eqv? requires at least two arguments");
    }

    while (cursor != LT_NIL){
        LT_Value next;
        LT_OBJECT_ARG(cursor, next);

        if (!LT_Value_eqv_p(first, next)){
            return LT_FALSE;
        }
    }

    return LT_TRUE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_equal_p,
    "equal?",
    "(left right ...)",
    "Return true when all arguments are structurally equal.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value first;

    LT_OBJECT_ARG(cursor, first);
    if (cursor == LT_NIL){
        LT_error("equal? requires at least two arguments");
    }

    while (cursor != LT_NIL){
        LT_Value next;
        LT_OBJECT_ARG(cursor, next);

        if (!LT_Value_equal_p(first, next)){
            return LT_FALSE;
        }
    }

    return LT_TRUE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_not,
    "not",
    "(value)",
    "Return true when value is falsey (nil or #false).",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Value_truthy_p(value) ? LT_FALSE : LT_TRUE;
}

LT_DEFINE_PRIMITIVE(
    primitive_gensym,
    "gensym",
    "([name])",
    "Return a fresh gensym or named uninterned symbol."
){
    LT_Value cursor = arguments;
    LT_Value name_designator = LT_NIL;
    char* name = NULL;

    LT_OBJECT_ARG_OPT(cursor, name_designator, LT_NIL);
    LT_ARG_END(cursor);
    if (name_designator != LT_NIL){
        if (LT_Symbol_p(name_designator)){
            name = LT_Symbol_name(LT_Symbol_from_value(name_designator));
        } else if (LT_String_p(name_designator)){
            name = (char*)LT_String_value_cstr(LT_String_from_value(name_designator));
        } else {
            LT_error("Name designator must be symbol or string");
        }
    }
    return LT_Symbol_gensym(name);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_null_p,
    "null?",
    "(value)",
    "Return true when value is nil.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return value == LT_NIL ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_boolean_p,
    "boolean?",
    "(value)",
    "Return true when value is a boolean.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Value_is_boolean(value) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_number_p,
    "number?",
    "(value)",
    "Return true when value is a number.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Number_value_p(value) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_symbol_p,
    "symbol?",
    "(value)",
    "Return true when value is a symbol.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Symbol_p(value) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_symbol_name,
    "symbol-name",
    "(value)",
    "Return symbol name as string.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    if (!LT_Symbol_p(value)){
        LT_type_error(value, &LT_Symbol_class);
    }
    return (LT_Value)(uintptr_t)LT_String_new_cstr(
        LT_Symbol_name(LT_Symbol_from_value(value))
    );
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_primitive_p,
    "primitive?",
    "(value)",
    "Return true when value is a primitive function.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Primitive_p(value) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_closure_p,
    "closure?",
    "(value)",
    "Return true when value is a closure.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Closure_p(value) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_macro_p,
    "macro?",
    "(value)",
    "Return true when value is a macro.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Macro_p(value) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_special_form_p,
    "special-form?",
    "(value)",
    "Return true when value is a special form.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_SpecialForm_p(value) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_class_p,
    "class?",
    "(value)",
    "Return true when value is a class object.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return class_object_p(value) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_environment_p,
    "environment?",
    "(value)",
    "Return true when value is an environment.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Environment_p(value) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    primitive_type_of,
    "type-of",
    "(value)",
    "Return class descriptor of value."
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_Value_class(value);
}

LT_DEFINE_PRIMITIVE(
    primitive_send,
    "send",
    "(receiver selector argument ...)",
    "Send selector to receiver with arguments."
){
    LT_Value cursor = arguments;
    LT_Value receiver;
    LT_Value selector;
    LT_Value message_arguments;

    LT_OBJECT_ARG(cursor, receiver);
    LT_OBJECT_ARG(cursor, selector);
    LT_ARG_REST(cursor, message_arguments);

    return LT_send(
        receiver,
        selector,
        message_arguments,
        tail_call_unwind_marker
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_slot_ref,
    "slot-ref",
    "(object slot)",
    "Read object slot value by symbol name."
){
    LT_Value cursor = arguments;
    LT_Value object;
    LT_Value slot_name;

    LT_OBJECT_ARG(cursor, object);
    LT_OBJECT_ARG(cursor, slot_name);
    LT_ARG_END(cursor);

    return LT_Object_slot_ref(object, slot_name);
}

LT_DEFINE_PRIMITIVE(
    primitive_slot_set,
    "slot-set!",
    "(object slot value)",
    "Set object slot value by symbol name."
){
    LT_Value cursor = arguments;
    LT_Value object;
    LT_Value slot_name;
    LT_Value value;

    LT_OBJECT_ARG(cursor, object);
    LT_OBJECT_ARG(cursor, slot_name);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);

    return LT_Object_slot_set(object, slot_name, value);
}

LT_DEFINE_PRIMITIVE(
    primitive_class_slots,
    "class-slots",
    "(class)",
    "Return list of slot names for class."
){
    LT_Value cursor = arguments;
    LT_Value class_value;
    LT_Class* klass;

    LT_OBJECT_ARG(cursor, class_value);
    LT_ARG_END(cursor);

    klass = LT_Class_from_object(class_value);
    return LT_Class_slots(klass);
}

LT_DEFINE_PRIMITIVE(
    primitive_make_class,
    "make-class",
    "(name superclasses slot-names)",
    "Create class with name, superclasses list, and slot-name list."
){
    LT_Value cursor = arguments;
    LT_Value name;
    LT_Value superclasses;
    LT_Value slot_names;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, name);
    LT_OBJECT_ARG(cursor, superclasses);
    LT_OBJECT_ARG(cursor, slot_names);
    LT_ARG_END(cursor);

    return LT_Class_new(name, superclasses, slot_names);
}

LT_DEFINE_PRIMITIVE(
    primitive_make_instance,
    "make-instance",
    "(class)",
    "Allocate empty instance for allocatable class."
){
    LT_Value cursor = arguments;
    LT_Value class_value;
    LT_Class* klass;

    LT_OBJECT_ARG(cursor, class_value);
    LT_ARG_END(cursor);

    klass = LT_Class_from_object(class_value);
    return LT_Class_make_instance(klass);
}

LT_DEFINE_PRIMITIVE(
    primitive_error,
    "error",
    "(message)",
    "Signal error condition and abort unless intercepted."
){
    LT_Value cursor = arguments;
    LT_Value message;

    LT_OBJECT_ARG(cursor, message);
    LT_ARG_END(cursor);

    if (!LT_String_p(message)){
        LT_type_error(message, &LT_String_class);
    }

    LT_error(LT_String_value_cstr(LT_String_from_value(message)));
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE(
    primitive_display,
    "display",
    "(value)",
    "Print value and newline to standard output, then return it."
){
    LT_Value cursor = arguments;
    LT_Value value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);

    if (LT_String_p(value)){
        fputs(LT_String_value_cstr(LT_String_from_value(value)), stdout);
    } else {
        LT_printer_print_object(value);
    }
    fputc('\n', stdout);
    fflush(stdout);
    return value;
}

LT_DEFINE_PRIMITIVE(
    primitive_read,
    "read",
    "()",
    "Read one line from standard input and return it as a string."
){
    LT_Value cursor = arguments;
    LT_StringBuilder* builder;
    int ch;
    (void)tail_call_unwind_marker;

    LT_ARG_END(cursor);

    builder = LT_StringBuilder_new();
    ch = fgetc(stdin);
    if (ch == EOF){
        return LT_NIL;
    }

    while (ch != EOF && ch != '\n'){
        if (ch != '\r'){
            LT_StringBuilder_append_char(builder, (char)ch);
        }
        ch = fgetc(stdin);
    }

    return (LT_Value)(uintptr_t)LT_String_new(
        LT_StringBuilder_value(builder),
        LT_StringBuilder_length(builder)
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_macroexpand,
    "macroexpand",
    "(form environment)",
    "Expand macros in form using environment object."
){
    LT_Value cursor = arguments;
    LT_Value expression;
    LT_Value environment_value;
    LT_Environment* lexical_environment;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, expression);
    LT_OBJECT_ARG(cursor, environment_value);
    LT_ARG_END(cursor);

    lexical_environment = LT_Environment_from_value(environment_value);
    return LT_compiler_macroexpand(expression, lexical_environment);
}

LT_DEFINE_PRIMITIVE(
    primitive_fold_expression,
    "fold-expression",
    "(form environment)",
    "Fold form with lexical constants from environment object."
){
    LT_Value cursor = arguments;
    LT_Value expression;
    LT_Value environment_value;
    LT_Environment* lexical_environment;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, expression);
    LT_OBJECT_ARG(cursor, environment_value);
    LT_ARG_END(cursor);

    lexical_environment = LT_Environment_from_value(environment_value);
    return LT_compiler_fold_expression(expression, lexical_environment);
}

LT_DEFINE_PRIMITIVE(
    primitive_define_package,
    "define-package",
    "(package-designator [used-package-or-(used-package nickname)] ...)",
    "Define package and optionally add used packages."
){
    LT_Value cursor = arguments;
    LT_Value package_designator;
    LT_Package* package;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, package_designator);
    package = package_from_designator(package_designator, 1);

    while (LT_Pair_p(cursor)){
        LT_Value spec = LT_car(cursor);

        if (LT_Pair_p(spec)){
            LT_Value spec_cursor = spec;
            LT_Value used_designator;
            LT_Value nickname_designator;

            LT_OBJECT_ARG(spec_cursor, used_designator);
            LT_OBJECT_ARG(spec_cursor, nickname_designator);
            LT_ARG_END(spec_cursor);

            LT_Package_use_package(
                package,
                package_from_designator(used_designator, 1),
                (char*)package_nickname_from_designator(nickname_designator)
            );
        } else {
            LT_Package_use_package(
                package,
                package_from_designator(spec, 1),
                NULL
            );
        }
        cursor = LT_cdr(cursor);
    }
    LT_ARG_END(cursor);

    return (LT_Value)(uintptr_t)package;
}

LT_DEFINE_PRIMITIVE(
    primitive_in_package,
    "in-package",
    "(package-designator)",
    "Set current package for symbol interning."
){
    LT_Value cursor = arguments;
    LT_Value package_designator;
    LT_Package* package;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, package_designator);
    LT_ARG_END(cursor);

    package = package_from_designator(package_designator, 0);
    LT_set_current_package(package);
    return (LT_Value)(uintptr_t)package;
}

LT_DEFINE_PRIMITIVE(
    primitive_use_package,
    "use-package",
    "(used-package-designator [nickname-designator])",
    "Use package in current package, optionally by nickname only."
){
    LT_Value cursor = arguments;
    LT_Value used_designator;
    LT_Value nickname_designator = LT_NIL;
    LT_Package* current_package;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, used_designator);
    LT_OBJECT_ARG_OPT(cursor, nickname_designator, LT_NIL);
    LT_ARG_END(cursor);

    current_package = LT_get_current_package();
    LT_Package_use_package(
        current_package,
        package_from_designator(used_designator, 0),
        (nickname_designator == LT_NIL)
            ? NULL
            : (char*)package_nickname_from_designator(nickname_designator)
    );
    return (LT_Value)(uintptr_t)current_package;
}

void LT_base_env_bind_primitives(LT_Environment* environment){
    LT_base_env_bind_static_primitive(environment, &primitive_numeric_equal);
    LT_base_env_bind_static_primitive(environment, &primitive_eq_p);
    LT_base_env_bind_static_primitive(environment, &primitive_eqv_p);
    LT_base_env_bind_static_primitive(environment, &primitive_equal_p);
    LT_base_env_bind_static_primitive(environment, &primitive_not);
    LT_base_env_bind_static_primitive(environment, &primitive_gensym);
    LT_base_env_bind_static_primitive(environment, &primitive_null_p);
    LT_base_env_bind_static_primitive(environment, &primitive_boolean_p);
    LT_base_env_bind_static_primitive(environment, &primitive_number_p);
    LT_base_env_bind_static_primitive(environment, &primitive_symbol_p);
    LT_base_env_bind_static_primitive(environment, &primitive_symbol_name);
    LT_base_env_bind_static_primitive(environment, &primitive_primitive_p);
    LT_base_env_bind_static_primitive(environment, &primitive_closure_p);
    LT_base_env_bind_static_primitive(environment, &primitive_macro_p);
    LT_base_env_bind_static_primitive(environment, &primitive_special_form_p);
    LT_base_env_bind_static_primitive(environment, &primitive_class_p);
    LT_base_env_bind_static_primitive(environment, &primitive_environment_p);
    LT_base_env_bind_static_primitive(environment, &primitive_type_of);
    LT_base_env_bind_static_primitive(environment, &primitive_send);
    LT_base_env_bind_static_primitive(environment, &primitive_class_slots);
    LT_base_env_bind_static_primitive(environment, &primitive_make_class);
    LT_base_env_bind_static_primitive(environment, &primitive_make_instance);
    LT_base_env_bind_static_primitive(environment, &primitive_slot_ref);
    LT_base_env_bind_static_primitive(environment, &primitive_slot_set);
    LT_base_env_bind_static_primitive(environment, &primitive_error);
    LT_base_env_bind_static_primitive(environment, &primitive_display);
    LT_base_env_bind_static_primitive(environment, &primitive_read);
    LT_base_env_bind_static_primitive(environment, &primitive_macroexpand);
    LT_base_env_bind_static_primitive(environment, &primitive_fold_expression);
    LT_base_env_bind_static_primitive(environment, &primitive_define_package);
    LT_base_env_bind_static_primitive(environment, &primitive_in_package);
    LT_base_env_bind_static_primitive(environment, &primitive_use_package);
}
