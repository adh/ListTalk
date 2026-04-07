/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Condition.h>
#include <ListTalk/classes/Printer.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/throw_catch.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef LT_HAVE_LIBEDIT
#ifdef LT_HAVE_EDITLINE_READLINE_H
#include <editline/readline.h>
#else
#include <readline/readline.h>
#endif
#endif

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

static void print_condition(LT_Value condition){
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
}

static LT_Value repl_error_handler(LT_Value arguments,
                                   LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value condition;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, condition);
    LT_ARG_END(cursor);

    if (!LT_IncompleteInputSyntaxError_p(condition)){
        print_condition(condition);
    }

    LT_throw(LT__repl_error_tag, condition);
}

static LT_Value script_error_handler(LT_Value arguments,
                                     LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value condition;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, condition);
    LT_ARG_END(cursor);

    print_condition(condition);

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

static int source_has_next_form(const char* source, size_t* offset){
    int ch = (unsigned char)source[*offset];

    while (1){
        while (ch != '\0' && isspace((unsigned char)ch)){
            (*offset)++;
            ch = (unsigned char)source[*offset];
        }

        if (ch == ';'){
            while (ch != '\0' && ch != '\n'){
                (*offset)++;
                ch = (unsigned char)source[*offset];
            }
            continue;
        }

        return ch != '\0';
    }
}

static int eval_source_string(const char* source,
                              LT_Value error_handler,
                              LT_Environment* environment,
                              int print_result,
                              LT_Value* caught_condition){
    size_t offset = 0;

    *caught_condition = LT_NIL;

    while (source_has_next_form(source, &offset)){
        LT_Reader* reader = LT_Reader_new();
        LT_ReaderStream* stream = LT_ReaderStream_newForString(source + offset);
        LT_Value object = LT_NIL;
        LT_Value result = LT_NIL;

        LT_CATCH(LT__repl_error_tag, *caught_condition, {
            LT_HANDLER_BIND(error_handler, {
                object = LT_Reader_readObject(reader, stream);
                result = LT_eval(object, environment, NULL);
                if (print_result){
                    LT_printer_print_object(result);
                    fputc('\n', stdout);
                }
            });
        });

        if (*caught_condition != LT_NIL){
            return 1;
        }

        offset += LT_ReaderStream_stringOffset(stream);
    }

    return 0;
}

static char* repl_read_line(const char* prompt){
#ifdef LT_HAVE_LIBEDIT
    return readline(prompt);
#else
    char* line = NULL;
    size_t capacity = 0;
    ssize_t length = getline(&line, &capacity, stdin);

    if (length < 0){
        free(line);
        return NULL;
    }

    return line;
#endif
}

static void repl_add_history_entry(const char* entry){
#ifdef LT_HAVE_LIBEDIT
    if (entry[0] != '\0'){
        add_history(entry);
    }
#else
    (void)entry;
#endif
}

static int eval_repl(LT_Value error_handler,
                     LT_Value eof_error_handler,
                     LT_Environment* environment){
    LT_StringBuilder* buffer = LT_StringBuilder_new();
    char* line;

    while ((line = repl_read_line(
        LT_StringBuilder_length(buffer) == 0 ? "listtalk> " : "      -> "
    )) != NULL){
        LT_Value caught = LT_NIL;
        int failed;
        size_t length = strlen(line);

        LT_StringBuilder_append_str(buffer, line);
        if (length == 0 || line[length - 1] != '\n'){
            LT_StringBuilder_append_char(buffer, '\n');
        }

        free(line);

        failed = eval_source_string(
            LT_StringBuilder_value(buffer),
            error_handler,
            environment,
            1,
            &caught
        );
        if (failed && LT_IncompleteInputSyntaxError_p(caught)){
            continue;
        }

        if (!failed){
            repl_add_history_entry(LT_StringBuilder_value(buffer));
        } else if (caught != LT_NIL){
            /* Error already reported by the REPL error handler. */
        }

        buffer = LT_StringBuilder_new();
    }

    if (LT_StringBuilder_length(buffer) > 0){
        LT_Value caught = LT_NIL;
        int failed = eval_source_string(
            LT_StringBuilder_value(buffer),
            eof_error_handler,
            environment,
            1,
            &caught
        );

        if (failed){
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
    LT_Value repl_handler;
    LT_Value file_handler;
    LT_Environment* base_environment;
    FILE* source_file;
    int eval_status;

    LT_init();
    LT_set_current_package(LT_PACKAGE_LISTTALK_USER);
    reader = LT_Reader_new();
    LT__repl_error_tag = LT_Symbol_new("repl-error");
    repl_handler = LT_Primitive_new(
        "repl-error-handler",
        "(condition)",
        "Print top-level REPL error and continue REPL loop.",
        repl_error_handler
    );
    file_handler = LT_Primitive_new(
        "script-error-handler",
        "(condition)",
        "Print top-level script error and stop execution.",
        script_error_handler
    );
    base_environment = LT_get_shared_base_environment();

    if (argc <= 1){
        (void)reader;
        return eval_repl(repl_handler, file_handler, base_environment);
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
            file_handler,
            base_environment,
            0,
            1
        );
        fclose(source_file);
    }
    return eval_status;
}
