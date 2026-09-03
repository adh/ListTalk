/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Condition.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Printer.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Restart.h>
#include <ListTalk/vm/reader.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/cmdopts.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/loader.h>
#include <ListTalk/vm/throw_catch.h>
#include <ListTalk/repl/repl.h>
#include <ListTalk/debugger/debugger.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static LT_Value LT__repl_error_tag = LT_NIL;
static LT_Value LT__return_to_toplevel_tag = LT_NIL;

LT_DEFINE_PRIMITIVE(
    return_to_toplevel_primitive,
    "return-to-toplevel",
    "()",
    "Abort the current computation and return to the interactive top level."
){
    LT_Value cursor = arguments;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_ARG_END(cursor);
    LT_throw(LT__return_to_toplevel_tag, LT_TRUE);
}

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

static LT_Value condition_handler_argument(LT_Value arguments){
    LT_Value cursor = arguments;
    LT_Value condition;

    LT_OBJECT_ARG(cursor, condition);
    LT_ARG_END(cursor);
    return condition;
}

static LT_Value throwing_error_handler(
    LT_Value arguments,
    LT_Value invocation_context_kind,
    LT_Value invocation_context_data,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
){
    LT_Value condition = condition_handler_argument(arguments);
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_invoke_debugger(condition);
    print_condition(condition);
    LT_throw(LT__repl_error_tag, condition);
}

static LT_Value repl_reader_error_handler(
    LT_Value arguments,
    LT_Value invocation_context_kind,
    LT_Value invocation_context_data,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
){
    LT_Value condition = condition_handler_argument(arguments);
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    print_condition(condition);
    if (LT_IncompleteInputSyntaxError_p(condition)){
        LT_throw(LT__repl_error_tag, condition);
    }
    return LT_NIL;
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
        LT_Reader* reader = LT_Reader_new(LT_NIL);
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

typedef struct {
    LT_Environment* environment;
    LT_Value error_handler;
    int status;
    int action_count;
    int no_std_lib;
    int standard_resolvers_initialized;
} CommandActionBaton;

static void prepend_standard_module_resolvers(LT_Environment* environment){
#ifdef LT_SOURCE_MODULE_DIR
    LT_base_environment_prepend_module_resolver(
        environment,
        LT_SOURCE_MODULE_DIR
    );
#endif
#ifdef LT_NATIVE_MODULE_DIR
    LT_base_environment_prepend_module_resolver(
        environment,
        LT_NATIVE_MODULE_DIR
    );
#endif
}

static void command_action_ensure_standard_resolvers(CommandActionBaton* action){
    if (action->no_std_lib || action->standard_resolvers_initialized){
        return;
    }

    prepend_standard_module_resolvers(action->environment);
    action->standard_resolvers_initialized = 1;
}

static char* command_action_module_name(char* value){
    if (value[0] == ':'){
        return value + 1;
    }
    return value;
}

static void eval_option_callback(LT_CmdOpts* parser, void* baton, char* value){
    CommandActionBaton* action = baton;
    LT_Value caught = LT_NIL;
    (void)parser;

    if (action->status != 0){
        return;
    }
    command_action_ensure_standard_resolvers(action);
    action->action_count++;
    action->status = eval_source_string(
        value,
        action->error_handler,
        action->environment,
        0,
        &caught
    );
}

static void eval_print_option_callback(LT_CmdOpts* parser,
                                       void* baton,
                                       char* value){
    CommandActionBaton* action = baton;
    LT_Value caught = LT_NIL;
    (void)parser;

    if (action->status != 0){
        return;
    }
    command_action_ensure_standard_resolvers(action);
    action->action_count++;
    action->status = eval_source_string(
        value,
        action->error_handler,
        action->environment,
        1,
        &caught
    );
}

static void load_option_callback(LT_CmdOpts* parser, void* baton, char* value){
    CommandActionBaton* action = baton;
    LT_Value caught = LT_NIL;
    LT_Value result = LT_NIL;
    (void)parser;

    if (action->status != 0){
        return;
    }
    command_action_ensure_standard_resolvers(action);
    action->action_count++;

    LT_CATCH(LT__repl_error_tag, caught, {
        LT_HANDLER_BIND(action->error_handler, {
            if (!LT_loader_load_file(value, action->environment, &result)){
                fprintf(stderr, "Error: unable to open source file '%s'\n", value);
                action->status = 1;
            }
        });
    });

    if (caught != LT_NIL){
        action->status = 1;
    }
}

static void require_option_callback(LT_CmdOpts* parser, void* baton, char* value){
    CommandActionBaton* action = baton;
    LT_Value caught = LT_NIL;
    char* module_name = command_action_module_name(value);
    (void)parser;

    if (action->status != 0){
        return;
    }
    command_action_ensure_standard_resolvers(action);
    action->action_count++;

    LT_CATCH(LT__repl_error_tag, caught, {
        LT_HANDLER_BIND(action->error_handler, {
            (void)LT_loader_require(
                action->environment,
                (LT_Value)(uintptr_t)LT_String_new_cstr(module_name)
            );
        });
    });

    if (caught != LT_NIL){
        action->status = 1;
    }
}

static void load_path_option_callback(LT_CmdOpts* parser,
                                      void* baton,
                                      char* value){
    CommandActionBaton* action = baton;
    (void)parser;

    if (action->status != 0){
        return;
    }
    command_action_ensure_standard_resolvers(action);
    LT_base_environment_prepend_module_resolver(action->environment, value);
}

static void no_std_lib_option_callback(LT_CmdOpts* parser,
                                       void* baton,
                                       char* value){
    CommandActionBaton* action = baton;
    (void)parser;
    (void)value;

    if (action->standard_resolvers_initialized || action->action_count != 0){
        LT_error("--no-std-lib must appear before environment-modifying options");
    }
    action->no_std_lib = 1;
}

static void debug_option_callback(LT_CmdOpts* parser,
                                  void* baton,
                                  char* value){
    (void)parser;
    (void)baton;
    (void)value;

    LT_Debugger_enable();
}

int main(int argc, char**argv){
    LT_Value repl_reader_handler;
    LT_Value file_handler;
    LT_Environment* base_environment;
    int eval_status;
    LT_CmdOpts* cmdopts;
    char* source_path = NULL;
    LT_Value command_line_list = LT_NIL;
    CommandActionBaton command_action;

    LT_INIT();
    LT_set_current_package(LT_PACKAGE_LISTTALK_USER);
    LT__repl_error_tag = LT_Symbol_new("repl-error");
    LT__return_to_toplevel_tag = LT_Symbol_new_uninterned(
        "return-to-toplevel"
    );
    repl_reader_handler = LT_Primitive_new(
        "repl-reader-error-handler",
        "(condition)",
        "Print top-level reader errors and continue the REPL when possible.",
        repl_reader_error_handler
    );
    file_handler = LT_Primitive_new(
        "script-error-handler",
        "(condition)",
        "Print top-level script error and stop execution.",
        throwing_error_handler
    );
    base_environment = LT_get_shared_base_environment();
    command_action.environment = base_environment;
    command_action.error_handler = file_handler;
    command_action.status = 0;
    command_action.action_count = 0;
    command_action.no_std_lib = 0;
    command_action.standard_resolvers_initialized = 0;

    cmdopts = LT_CmdOpts_new(LT_CMDOPTS_STRICT_ORDER);
    LT_CmdOpts_addOption(
        cmdopts,
        0,
        'd',
        "debug",
        debug_option_callback,
        NULL
    );
    LT_CmdOpts_addOption(
        cmdopts,
        1,
        'e',
        "eval",
        eval_option_callback,
        &command_action
    );
    LT_CmdOpts_addOption(
        cmdopts,
        1,
        'E',
        "eval-print",
        eval_print_option_callback,
        &command_action
    );
    LT_CmdOpts_addOption(
        cmdopts,
        1,
        'l',
        "load",
        load_option_callback,
        &command_action
    );
    LT_CmdOpts_addOption(
        cmdopts,
        1,
        'r',
        "require",
        require_option_callback,
        &command_action
    );
    LT_CmdOpts_addOption(
        cmdopts,
        1,
        'L',
        "load-path",
        load_path_option_callback,
        &command_action
    );
    LT_CmdOpts_addOption(
        cmdopts,
        0,
        '\0',
        "no-std-lib",
        no_std_lib_option_callback,
        &command_action
    );
    LT_CmdOpts_addStringArgument(cmdopts, 0, &source_path);
    LT_CmdOpts_addStringListArgument(cmdopts, 0, &command_line_list);
    LT_CmdOpts_parseArgv(cmdopts, argc - 1, argv + 1);
    if (command_action.status != 0){
        return command_action.status;
    }

    if (source_path == NULL){
        if (command_action.action_count != 0){
            return 0;
        }
        command_action_ensure_standard_resolvers(&command_action);
        LT_enable_KeyboardInterrupt();
        LT_Debugger_enable();
        {
            LT_REPL_State* repl = LT_REPL_State_new();
            LT_Value return_to_toplevel_restart = LT_Restart_new(
                LT_Symbol_new_in(LT_PACKAGE_KEYWORD, "return-to-toplevel"),
                (LT_Value)(uintptr_t)LT_String_new_cstr(
                    "Abort the current computation and return to the top level."
                ),
                LT_NIL,
                LT_Primitive_from_static(&return_to_toplevel_primitive)
            );

            eval_status = 0;
            while (1){
                LT_Value reader_error = LT_NIL;
                LT_Value returned_to_toplevel = LT_NIL;
                LT_Value object = LT_INVALID;

                LT_CATCH(
                    LT__return_to_toplevel_tag,
                    returned_to_toplevel,
                    {
                        LT_RESTART_BIND(return_to_toplevel_restart, {
                            LT_CATCH(LT__repl_error_tag, reader_error, {
                                LT_HANDLER_BIND(repl_reader_handler, {
                                    object = LT_REPL_State_read(repl);
                                });
                            });

                            if (reader_error == LT_NIL
                                && object != LT_INVALID){
                                LT_Value result = LT_eval(
                                    object,
                                    base_environment,
                                    NULL
                                );

                                LT_printer_print_object(result);
                                fputc('\n', stdout);
                            }
                        });
                    }
                );

                if (returned_to_toplevel == LT_NIL){
                    if (reader_error != LT_NIL){
                        continue;
                    }
                    if (object == LT_INVALID){
                        break;
                    }
                }
            }
        }
        LT_disable_KeyboardInterrupt();
        return eval_status;
    }
    command_action_ensure_standard_resolvers(&command_action);
    command_line_list = LT_cons(
        (LT_Value)(uintptr_t)LT_String_new_cstr(source_path),
        command_line_list
    );

    {
        LT_Value command_line_symbol = LT_Symbol_new_in(
            LT_PACKAGE_LISTTALK,
            "command-line"
        );

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
