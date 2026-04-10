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
#include <ListTalk/cmdopts.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/loader.h>
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
                                   LT_Value invocation_context_kind,
                                   LT_Value invocation_context_data,
                                   LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value condition;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, condition);
    LT_ARG_END(cursor);

    if (!LT_IncompleteInputSyntaxError_p(condition)){
        print_condition(condition);
    }

    LT_throw(LT__repl_error_tag, condition);
}

static LT_Value script_error_handler(LT_Value arguments,
                                     LT_Value invocation_context_kind,
                                     LT_Value invocation_context_data,
                                     LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value condition;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, condition);
    LT_ARG_END(cursor);

    print_condition(condition);

    LT_throw(LT__repl_error_tag, condition);
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

static void load_path_option_callback(LT_CmdOpts* parser,
                                      void* baton,
                                      char* value){
    LT_Environment* environment = baton;
    (void)parser;

    LT_base_environment_prepend_module_resolver(environment, value);
}

int main(int argc, char**argv){
    LT_Value repl_handler;
    LT_Value file_handler;
    LT_Environment* base_environment;
    int eval_status;
    LT_CmdOpts* cmdopts;
    char* source_path = NULL;
    LT_Value command_line_list = LT_NIL;

    LT_init();
    LT_set_current_package(LT_PACKAGE_LISTTALK_USER);
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

    cmdopts = LT_CmdOpts_new(LT_CMDOPTS_STRICT_ORDER);
    LT_CmdOpts_addOption(
        cmdopts,
        1,
        'L',
        "load-path",
        load_path_option_callback,
        base_environment
    );
    LT_CmdOpts_addStringArgument(cmdopts, 0, &source_path);
    LT_CmdOpts_addStringListArgument(cmdopts, 0, &command_line_list);
    LT_CmdOpts_parseArgv(cmdopts, argc - 1, argv + 1);

    if (source_path == NULL){
        return eval_repl(repl_handler, file_handler, base_environment);
    }

    {
        LT_Value command_line_symbol = LT_Symbol_new("*command-line*");

        LT_Environment_bind(
            base_environment,
            command_line_symbol,
            command_line_list,
            0
        );

        LT_Value caught = LT_NIL;
        LT_Value result = LT_NIL;

        LT_CATCH(LT__repl_error_tag, caught, {
            LT_HANDLER_BIND(file_handler, {
                if (!LT_loader_load_file(
                    source_path,
                    base_environment,
                    &result
                )){
                    fprintf(
                        stderr,
                        "Error: unable to open source file '%s'\n",
                        source_path
                    );
                    eval_status = 1;
                } else {
                    eval_status = 0;
                }
            });
        });

        if (caught != LT_NIL){
            eval_status = 1;
        }
        if (eval_status != 0){
            return 1;
        }
    }
    return eval_status;
}
