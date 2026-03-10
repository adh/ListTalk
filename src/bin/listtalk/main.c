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

    LT_throw(LT__repl_error_tag, condition);
}

int main(int argc, char**argv){
    LT_Reader* reader;
    LT_ReaderStream* stream;
    LT_Value error_handler;

    (void)argc;
    (void)argv;
    LT_init();
    reader = LT_Reader_new();
    stream = LT_ReaderStream_newForFile(stdin);
    LT__repl_error_tag = LT_Symbol_new("repl-error");
    error_handler = LT_Primitive_new(
        "repl-error-handler",
        "(condition)",
        "Print top-level error and continue REPL loop.",
        repl_error_handler
    );

    while (stream_has_next_form(stdin)){
        LT_Value object;
        LT_Value result;
        LT_Value caught = LT_NIL;

        LT_CATCH(LT__repl_error_tag, caught, {
            LT_HANDLER_BIND(error_handler, {
                object = LT_Reader_readObject(reader, stream);
                result = LT_eval(object, LT_get_shared_base_environment(), NULL);
                LT_printer_print_object(result);
                fputc('\n', stdout);
            });
        });
        (void)caught;
    }

    return 0;
}
