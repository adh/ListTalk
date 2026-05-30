/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Condition.h>
#include <ListTalk/classes/Pair.h>
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

#ifdef LT_HAVE_LIBEDIT
typedef struct {
    char** values;
    size_t count;
    size_t capacity;
    size_t index;
} ReplCompletionMatches;

static ReplCompletionMatches repl_completion_matches = {0};

static void repl_completion_matches_clear(void){
    size_t i;

    for (i = 0; i < repl_completion_matches.count; i++){
        free(repl_completion_matches.values[i]);
    }
    free(repl_completion_matches.values);
    repl_completion_matches.values = NULL;
    repl_completion_matches.count = 0;
    repl_completion_matches.capacity = 0;
    repl_completion_matches.index = 0;
}

static int cstr_starts_with(const char* value, const char* prefix){
    size_t prefix_length = strlen(prefix);

    return strncmp(value, prefix, prefix_length) == 0;
}

static int repl_completion_reader_delimiter_p(int ch){
    return ch == '\0'
        || isspace((unsigned char)ch)
        || ch == '('
        || ch == ')'
        || ch == '['
        || ch == ']'
        || ch == '{'
        || ch == '}'
        || ch == '"'
        || ch == '\''
        || ch == '`'
        || ch == ','
        || ch == ';';
}

static int repl_completion_symbol_name_completable_p(char* name, int allow_colon){
    char* cursor = name;

    while (*cursor != '\0'){
        if (repl_completion_reader_delimiter_p((unsigned char)*cursor)
            || *cursor == '\\'
            || *cursor == '|'
            || (!allow_colon && *cursor == ':')){
            return 0;
        }
        cursor++;
    }
    return 1;
}

static void repl_completion_matches_append_owned(char* value){
    size_t i;
    char** values;

    for (i = 0; i < repl_completion_matches.count; i++){
        if (strcmp(repl_completion_matches.values[i], value) == 0){
            free(value);
            return;
        }
    }

    if (repl_completion_matches.count == repl_completion_matches.capacity){
        size_t capacity = repl_completion_matches.capacity == 0
            ? 16
            : repl_completion_matches.capacity * 2;
        values = realloc(
            repl_completion_matches.values,
            sizeof(char*) * capacity
        );
        if (values == NULL){
            LT_error("Could not allocate REPL completion matches");
        }
        repl_completion_matches.values = values;
        repl_completion_matches.capacity = capacity;
    }

    repl_completion_matches.values[repl_completion_matches.count++] = value;
}

static void repl_completion_matches_append_cstr(const char* value){
    char* copy = strdup(value);

    if (copy == NULL){
        LT_error("Could not allocate REPL completion match");
    }
    repl_completion_matches_append_owned(copy);
}

static void repl_completion_add_symbol_name(char* prefix,
                                            LT_Value symbol,
                                            const char* package_prefix,
                                            int allow_colon){
    char* name = LT_Symbol_name(LT_Symbol_from_value(symbol));

    if (!repl_completion_symbol_name_completable_p(name, allow_colon)){
        return;
    }
    if (!cstr_starts_with(name, prefix)){
        return;
    }
    if (package_prefix == NULL){
        repl_completion_matches_append_cstr(name);
    } else {
        repl_completion_matches_append_cstr(LT_sprintf(
            "%s%s",
            package_prefix,
            name
        ));
    }
}

static void repl_completion_add_package_name(char* prefix, LT_Package* package){
    char* name = LT_Package_name(package);

    if (cstr_starts_with(name, prefix)){
        repl_completion_matches_append_cstr(LT_sprintf("%s:", name));
    }
}

static void repl_completion_add_package_symbols(char* prefix,
                                                LT_Package* package,
                                                const char* package_prefix,
                                                int allow_colon){
    LT_Value symbols = LT_Package_symbols_asList(package);

    while (LT_Pair_p(symbols)){
        repl_completion_add_symbol_name(
            prefix,
            LT_car(symbols),
            package_prefix,
            allow_colon
        );
        symbols = LT_cdr(symbols);
    }
    if (symbols != LT_NIL){
        LT_error("Package symbolsAsList must return a proper list");
    }
}

static void repl_completion_add_accessible_symbols(char* prefix){
    LT_Package* current_package = LT_get_current_package();
    LT_Value used_packages;

    repl_completion_add_package_symbols(prefix, current_package, NULL, 0);
    used_packages = LT_Package_used_packages(current_package);
    while (LT_Pair_p(used_packages)){
        repl_completion_add_package_symbols(
            prefix,
            (LT_Package*)LT_VALUE_POINTER_VALUE(LT_car(used_packages)),
            NULL,
            0
        );
        used_packages = LT_cdr(used_packages);
    }
    if (used_packages != LT_NIL){
        LT_error("Package used-packages must be proper list");
    }
}

static void repl_completion_add_packages(char* prefix){
    LT_Value packages = LT_Package_packages_asList();

    while (LT_Pair_p(packages)){
        repl_completion_add_package_name(
            prefix,
            LT_Package_from_value(LT_car(packages))
        );
        packages = LT_cdr(packages);
    }
    if (packages != LT_NIL){
        LT_error("Package packagesAsList must return a proper list");
    }
}

static void repl_completion_add_keywords(char* text){
    repl_completion_add_package_symbols(text + 1, LT_PACKAGE_KEYWORD, ":", 1);
}

static char* repl_completion_package_designator(char* text, char* colon){
    size_t length = (size_t)(colon - text);
    char* designator = malloc(length + 1);

    if (designator == NULL){
        LT_error("Could not allocate REPL completion package designator");
    }
    memcpy(designator, text, length);
    designator[length] = '\0';
    return designator;
}

static void repl_completion_add_qualified_symbols(char* text, char* colon){
    char* package_designator = repl_completion_package_designator(text, colon);
    LT_Package* package = LT_Package_resolve_used_package(
        LT_get_current_package(),
        package_designator
    );

    if (package == NULL){
        package = LT_Package_find(package_designator);
    }
    if (package != NULL){
        char* package_prefix = LT_sprintf("%s:", package_designator);

        repl_completion_add_package_symbols(
            colon + 1,
            package,
            package_prefix,
            0
        );
    }
    free(package_designator);
}

static void repl_completion_build_matches(char* text){
    char* colon = strrchr(text, ':');

    repl_completion_matches_clear();
    if (text[0] == ':'){
        repl_completion_add_keywords(text);
    } else if (colon != NULL){
        repl_completion_add_qualified_symbols(text, colon);
    } else {
        repl_completion_add_packages(text);
        repl_completion_add_accessible_symbols(text);
    }
}

static char* repl_completion_generator(const char* text, int state){
    if (state == 0){
        repl_completion_build_matches((char*)text);
    }

    if (repl_completion_matches.index >= repl_completion_matches.count){
        return NULL;
    }
    return strdup(
        repl_completion_matches.values[repl_completion_matches.index++]
    );
}

static int repl_completion_token_char_p(int ch){
    return !repl_completion_reader_delimiter_p(ch);
}

static char** repl_attempted_completion(const char* text, int start, int end){
    (void)text;
    (void)end;

    if (start > 0 && repl_completion_token_char_p(rl_line_buffer[start - 1])){
        return NULL;
    }
    rl_attempted_completion_over = 1;
    return rl_completion_matches(
        rl_line_buffer + start,
        repl_completion_generator
    );
}

static void repl_install_completion(void){
    rl_attempted_completion_function = repl_attempted_completion;
    rl_completer_word_break_characters = " \t\n()[]{}'`\",;";
    rl_completion_append_character = '\0';
}
#endif

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

        repl_add_history_entry(LT_StringBuilder_value(buffer));

        buffer = LT_StringBuilder_new();
    }

    puts(""); /* new line after ^D */
 
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

int main(int argc, char**argv){
    LT_Value repl_handler;
    LT_Value file_handler;
    LT_Environment* base_environment;
    int eval_status;
    LT_CmdOpts* cmdopts;
    char* source_path = NULL;
    LT_Value command_line_list = LT_NIL;
    CommandActionBaton command_action;

    LT_INIT();
    LT_set_current_package(LT_PACKAGE_LISTTALK_USER);
#ifdef LT_HAVE_LIBEDIT
    repl_install_completion();
#endif
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
    command_action.environment = base_environment;
    command_action.error_handler = file_handler;
    command_action.status = 0;
    command_action.action_count = 0;
    command_action.no_std_lib = 0;
    command_action.standard_resolvers_initialized = 0;

    cmdopts = LT_CmdOpts_new(LT_CMDOPTS_STRICT_ORDER);
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
        return eval_repl(repl_handler, file_handler, base_environment);
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
