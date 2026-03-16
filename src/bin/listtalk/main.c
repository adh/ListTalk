/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Printer.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/throw_catch.h>
#include <ctype.h>
#include <stdio.h>

/* this does not belong here but in the actual reader implementation,
 * but implementation in reader would require working condition system, 
 * so here it is for now */
static int stream_has_next_form(FILE* stream){
    int ch = fgetc(stream);

    while (1){
        while (ch != EOF && isspace((unsigned char)ch)){
            ch = fgetc(stream);
        }

        if (ch == ';'){
            while (ch != EOF && ch != '\n'){
                ch = fgetc(stream);
            }
            ch = fgetc(stream);
            continue;
        }

        if (ch == EOF){
            return 0;
        }

        ungetc(ch, stream);
        return 1;
    }
}

static LT_Value LT__repl_error_tag = LT_NIL;

static LT_Value repl_error_handler(LT_Value arguments,
                                   LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value condition;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, condition);
    LT_ARG_END(cursor);

    if (LT_Value_class(condition) == &LT_String_class){
        fprintf(
            stderr,
            "Error: %s\n",
            LT_String_value_cstr(LT_String_from_value(condition))
        );
    } else {
        fputs("Error: ", stderr);
        LT_Value_debugPrintOn(condition, stderr);
        fputc('\n', stderr);
    }
    LT_print_backtrace(stderr);

    LT_throw(LT__repl_error_tag, condition);
}

static int eval_stream(FILE* file,
                       LT_Reader* reader,
                       LT_Value error_handler,
                       LT_Environment* environment,
                       int print_result,
                       int stop_on_error){
    LT_ReaderStream* stream = LT_ReaderStream_newForFile(file);

    while (stream_has_next_form(file)){
        LT_Value object;
        LT_Value result;
        LT_Value caught = LT_NIL;

        LT_CATCH(LT__repl_error_tag, caught, {
            LT_HANDLER_BIND(error_handler, {
                object = LT_Reader_readObject(reader, stream);
                result = LT_eval(object, environment, NULL);
                if (print_result){
                    LT_printer_print_object(result);
                    fputc('\n', stdout);
                }
            });
        });

        if (caught != LT_NIL && stop_on_error){
            return 1;
        }
    }

    return 0;
}

static LT_Value build_command_line_list(int argc, char** argv){
    LT_Value args = LT_NIL;
    int index;

    for (index = argc - 1; index >= 1; index--){
        LT_Value argument = (LT_Value)(uintptr_t)LT_String_new_cstr(argv[index]);
        args = LT_cons(argument, args);
    }

    return args;
}

int main(int argc, char**argv){
    LT_Reader* reader;
    LT_Value error_handler;
    LT_Environment* base_environment;
    FILE* source_file;
    int eval_status;

    LT_init();
    LT_set_current_package(LT_PACKAGE_LISTTALK_USER);
    reader = LT_Reader_new();
    LT__repl_error_tag = LT_Symbol_new("repl-error");
    error_handler = LT_Primitive_new(
        "repl-error-handler",
        "(condition)",
        "Print top-level error and continue REPL loop.",
        repl_error_handler
    );
    base_environment = LT_get_shared_base_environment();

    if (argc <= 1){
        return eval_stream(
            stdin,
            reader,
            error_handler,
            base_environment,
            1,
            0
        );
    }

    {
        LT_Value command_line_symbol = LT_Symbol_new("*command-line*");
        LT_Value command_line_list = build_command_line_list(argc, argv);

        LT_Environment_bind(
            base_environment,
            command_line_symbol,
            command_line_list,
            0
        );

        source_file = fopen(argv[1], "r");
        if (source_file == NULL){
            fprintf(stderr, "Error: unable to open source file '%s'\n", argv[1]);
            return 1;
        }

        eval_status = eval_stream(
            source_file,
            reader,
            error_handler,
            base_environment,
            0,
            1
        );
        fclose(source_file);
    }
    return eval_status;
}
