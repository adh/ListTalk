/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/cmdopts.h>
#include <ListTalk/classes/Character.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/loader.h>

#include <stdint.h>
#include <string.h>

LT_DECLARE_CLASS(LT_CmdOpts_Parser);

struct LT_CmdOpts_Parser_s {
    LT_Object base;
    LT_CmdOpts* parser;
};

typedef struct {
    LT_CmdOpts_Parser* parser;
    LT_Value callback;
} CmdOptsCallbackBaton;

static LT_CmdOpts_Parser* cmdopts_parser_new(int flags){
    LT_CmdOpts_Parser* parser = LT_Class_ALLOC(LT_CmdOpts_Parser);

    parser->parser = LT_CmdOpts_new(flags);
    return parser;
}

static char cmdopts_short_option_from_value(LT_Value value){
    uint32_t codepoint;

    if (value == LT_NIL){
        return '\0';
    }
    if (LT_String_p(value)){
        LT_String* string = LT_String_from_value(value);

        if (LT_String_length(string) != 1){
            LT_error("Short option string must contain one character");
        }
        codepoint = LT_String_at(string, 0);
    } else {
        codepoint = LT_Character_value(value);
    }

    if (codepoint > (uint32_t)CHAR_MAX){
        LT_error("Short option must be an ASCII character");
    }
    return (char)codepoint;
}

static char* cmdopts_long_option_from_value(LT_Value value){
    if (value == LT_NIL){
        return NULL;
    }
    return (char*)LT_String_value_cstr(LT_String_from_value(value));
}

static int cmdopts_keyword_p(LT_Value value, char* name){
    LT_Symbol* symbol;

    if (!LT_Symbol_p(value)){
        LT_type_error(value, &LT_Symbol_class);
    }
    symbol = LT_Symbol_from_value(value);
    return LT_Symbol_package(symbol) == LT_PACKAGE_KEYWORD
        && strcmp(LT_Symbol_name(symbol), name) == 0;
}

static int cmdopts_parser_flags_from_keywords(LT_Value keywords){
    LT_Value cursor = keywords;
    int flags = 0;

    while (cursor != LT_NIL){
        LT_Value keyword;

        if (!LT_Pair_p(cursor)){
            LT_error("Parser flags must be a proper list");
        }
        keyword = LT_car(cursor);
        if (cmdopts_keyword_p(keyword, "strict-order")){
            flags |= LT_CMDOPTS_STRICT_ORDER;
        } else {
            LT_error("Unknown parser flag");
        }
        cursor = LT_cdr(cursor);
    }
    return flags;
}

static int cmdopts_argument_flags_from_keywords(LT_Value keywords){
    LT_Value cursor = keywords;
    int flags = 0;

    while (cursor != LT_NIL){
        LT_Value keyword;

        if (!LT_Pair_p(cursor)){
            LT_error("Argument flags must be a proper list");
        }
        keyword = LT_car(cursor);
        if (cmdopts_keyword_p(keyword, "required")){
            flags |= LT_CMDOPTS_ARGUMENT_REQUIRED;
        } else if (cmdopts_keyword_p(keyword, "multiple")){
            flags |= LT_CMDOPTS_ARGUMENT_MULTIPLE;
        } else {
            LT_error("Unknown argument flag");
        }
        cursor = LT_cdr(cursor);
    }
    return flags;
}

static void cmdopts_callback(LT_CmdOpts* c_parser, void* baton, char* value){
    CmdOptsCallbackBaton* callback_baton = baton;
    LT_Value parser;
    LT_Value argument;
    (void)c_parser;

    parser = (LT_Value)(uintptr_t)callback_baton->parser;
    argument = value == NULL
        ? LT_NIL
        : (LT_Value)(uintptr_t)LT_String_new_cstr(value);
    LT_apply(
        callback_baton->callback,
        LT_listn(2, parser, argument),
        LT_NIL,
        LT_NIL,
        NULL
    );
}

static CmdOptsCallbackBaton* cmdopts_callback_baton(LT_CmdOpts_Parser* parser,
                                                    LT_Value callback){
    CmdOptsCallbackBaton* baton = GC_NEW(CmdOptsCallbackBaton);

    baton->parser = parser;
    baton->callback = callback;
    return baton;
}

static void CmdOpts_Parser_debugPrintOn(LT_Value obj, FILE* stream){
    LT_CmdOpts_Parser* parser = LT_CmdOpts_Parser_from_value(obj);

    fprintf(stream, "#<Parser %p>", (void*)parser);
}

LT_DEFINE_PRIMITIVE(
    cmdopts_parser_class_method_new,
    "Parser class>>new",
    "(self)",
    "Create a command line parser."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    if (self != LT_STATIC_CLASS(LT_CmdOpts_Parser)){
        LT_error("new class method is only supported on Parser");
    }
    return (LT_Value)(uintptr_t)cmdopts_parser_new(0);
}

LT_DEFINE_PRIMITIVE(
    cmdopts_parser_class_method_new_with_flags,
    "Parser class>>newWithFlags:",
    "(self flags)",
    "Create a command line parser with a list of keyword flags."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value flag_keywords;
    int flags;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, flag_keywords);
    LT_ARG_END(cursor);
    if (self != LT_STATIC_CLASS(LT_CmdOpts_Parser)){
        LT_error("newWithFlags: class method is only supported on Parser");
    }
    flags = cmdopts_parser_flags_from_keywords(flag_keywords);
    return (LT_Value)(uintptr_t)cmdopts_parser_new(flags);
}

LT_DEFINE_PRIMITIVE(
    cmdopts_parser_method_add_option,
    "Parser>>addOption:shortOption:longOption:do:",
    "(self hasArgument shortOption longOption callback)",
    "Add an option with a callback receiving the parser and string or nil value."
){
    LT_Value cursor = arguments;
    LT_CmdOpts_Parser* self;
    LT_Value has_argument;
    LT_Value short_option_value;
    LT_Value long_option_value;
    LT_Value callback;
    char short_option;
    char* long_option;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_CmdOpts_Parser*, LT_CmdOpts_Parser_from_value);
    LT_OBJECT_ARG(cursor, has_argument);
    LT_OBJECT_ARG(cursor, short_option_value);
    LT_OBJECT_ARG(cursor, long_option_value);
    LT_OBJECT_ARG(cursor, callback);
    LT_ARG_END(cursor);

    short_option = cmdopts_short_option_from_value(short_option_value);
    long_option = cmdopts_long_option_from_value(long_option_value);
    LT_CmdOpts_addOption(
        self->parser,
        LT_Value_truthy_p(has_argument),
        short_option,
        long_option,
        cmdopts_callback,
        cmdopts_callback_baton(self, callback)
    );
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    cmdopts_parser_method_add_argument,
    "Parser>>addArgument:do:",
    "(self flags callback)",
    "Add a positional argument with keyword flags and a callback receiving the parser and string value."
){
    LT_Value cursor = arguments;
    LT_CmdOpts_Parser* self;
    LT_Value flag_keywords;
    LT_Value callback;
    int flags;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_CmdOpts_Parser*, LT_CmdOpts_Parser_from_value);
    LT_OBJECT_ARG(cursor, flag_keywords);
    LT_OBJECT_ARG(cursor, callback);
    LT_ARG_END(cursor);

    flags = cmdopts_argument_flags_from_keywords(flag_keywords);
    LT_CmdOpts_addArgument(
        self->parser,
        flags,
        cmdopts_callback,
        cmdopts_callback_baton(self, callback)
    );
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    cmdopts_parser_method_parse_list,
    "Parser>>parseList:",
    "(self arguments)",
    "Parse a ListTalk list of command line argument strings."
){
    LT_Value cursor = arguments;
    LT_CmdOpts_Parser* self;
    LT_Value list;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_CmdOpts_Parser*, LT_CmdOpts_Parser_from_value);
    LT_OBJECT_ARG(cursor, list);
    LT_ARG_END(cursor);

    LT_CmdOpts_parseList(self->parser, list);
    return (LT_Value)(uintptr_t)self;
}

static LT_Method_Descriptor CmdOpts_Parser_methods[] = {
    {"addOption:shortOption:longOption:do:", &cmdopts_parser_method_add_option},
    {"addArgument:do:", &cmdopts_parser_method_add_argument},
    {"parseList:", &cmdopts_parser_method_parse_list},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor CmdOpts_Parser_class_methods[] = {
    {"new", &cmdopts_parser_class_method_new},
    {"newWithFlags:", &cmdopts_parser_class_method_new_with_flags},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_CmdOpts_Parser) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Parser",
    .documentation = "Callback-based command line option parser.",
    .instance_size = sizeof(LT_CmdOpts_Parser),
    .class_flags = LT_CLASS_FLAG_FINAL,
    .debugPrintOn = CmdOpts_Parser_debugPrintOn,
    .methods = CmdOpts_Parser_methods,
    .class_methods = CmdOpts_Parser_class_methods,
};

void ListTalk_cmdopts_load(LT_Environment* environment){
    LT_Package* package = LT_Package_new("ListTalk-cmdopts");

    LT_init_native_class(&LT_CmdOpts_Parser_class);
    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(package, "Parser"),
        LT_STATIC_CLASS(LT_CmdOpts_Parser),
        LT_ENV_BINDING_FLAG_CONSTANT
    );

    LT_loader_provide(environment, "cmdopts");
}
