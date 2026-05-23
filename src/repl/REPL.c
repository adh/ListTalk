/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/REPL.h>

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Condition.h>
#include <ListTalk/classes/Printer.h>
#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/stack_trace.h>
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

static LT_Value LTREPL_error_tag = LT_NIL;

#ifdef LT_HAVE_LIBEDIT
typedef struct {
    char** values;
    size_t count;
    size_t capacity;
    size_t index;
} LTREPL_CompletionMatches;

static LTREPL_CompletionMatches LTREPL_completion_matches = {0};

static void LTREPL_completion_matches_clear(void){
    size_t i;

    for (i = 0; i < LTREPL_completion_matches.count; i++){
        free(LTREPL_completion_matches.values[i]);
    }
    free(LTREPL_completion_matches.values);
    LTREPL_completion_matches.values = NULL;
    LTREPL_completion_matches.count = 0;
    LTREPL_completion_matches.capacity = 0;
    LTREPL_completion_matches.index = 0;
}

static int LTREPL_cstr_starts_with(const char* value, const char* prefix){
    size_t prefix_length = strlen(prefix);

    return strncmp(value, prefix, prefix_length) == 0;
}

static int LTREPL_completion_reader_delimiter_p(int ch){
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

static int LTREPL_completion_symbol_name_completable_p(
    char* name,
    int allow_colon
){
    char* cursor = name;

    while (*cursor != '\0'){
        if (LTREPL_completion_reader_delimiter_p((unsigned char)*cursor)
            || *cursor == '\\'
            || *cursor == '|'
            || (!allow_colon && *cursor == ':')){
            return 0;
        }
        cursor++;
    }
    return 1;
}

static void LTREPL_completion_matches_append_owned(char* value){
    size_t i;
    char** values;

    for (i = 0; i < LTREPL_completion_matches.count; i++){
        if (strcmp(LTREPL_completion_matches.values[i], value) == 0){
            free(value);
            return;
        }
    }

    if (LTREPL_completion_matches.count == LTREPL_completion_matches.capacity){
        size_t capacity = LTREPL_completion_matches.capacity == 0
            ? 16
            : LTREPL_completion_matches.capacity * 2;
        values = realloc(
            LTREPL_completion_matches.values,
            sizeof(char*) * capacity
        );
        if (values == NULL){
            LT_error("Could not allocate REPL completion matches");
        }
        LTREPL_completion_matches.values = values;
        LTREPL_completion_matches.capacity = capacity;
    }

    LTREPL_completion_matches.values[LTREPL_completion_matches.count++] = value;
}

static void LTREPL_completion_matches_append_cstr(const char* value){
    char* copy = strdup(value);

    if (copy == NULL){
        LT_error("Could not allocate REPL completion match");
    }
    LTREPL_completion_matches_append_owned(copy);
}

static void LTREPL_completion_add_symbol_name(char* prefix,
                                              LT_Value symbol,
                                              const char* package_prefix,
                                              int allow_colon){
    char* name = LT_Symbol_name(LT_Symbol_from_value(symbol));

    if (!LTREPL_completion_symbol_name_completable_p(name, allow_colon)){
        return;
    }
    if (!LTREPL_cstr_starts_with(name, prefix)){
        return;
    }
    if (package_prefix == NULL){
        LTREPL_completion_matches_append_cstr(name);
    } else {
        LTREPL_completion_matches_append_cstr(LT_sprintf(
            "%s%s",
            package_prefix,
            name
        ));
    }
}

static void LTREPL_completion_add_package_name(char* prefix,
                                               LT_Package* package){
    char* name = LT_Package_name(package);

    if (LTREPL_cstr_starts_with(name, prefix)){
        LTREPL_completion_matches_append_cstr(LT_sprintf("%s:", name));
    }
}

static void LTREPL_completion_add_package_symbols(char* prefix,
                                                  LT_Package* package,
                                                  const char* package_prefix,
                                                  int allow_colon){
    LT_Value symbols = LT_Package_symbols_asList(package);

    while (LT_Pair_p(symbols)){
        LTREPL_completion_add_symbol_name(
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

static void LTREPL_completion_add_accessible_symbols(char* prefix){
    LT_Package* current_package = LT_get_current_package();
    LT_Value used_packages;

    LTREPL_completion_add_package_symbols(prefix, current_package, NULL, 0);
    used_packages = LT_Package_used_packages(current_package);
    while (LT_Pair_p(used_packages)){
        LTREPL_completion_add_package_symbols(
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

static void LTREPL_completion_add_packages(char* prefix){
    LT_Value packages = LT_Package_packages_asList();

    while (LT_Pair_p(packages)){
        LTREPL_completion_add_package_name(
            prefix,
            LT_Package_from_value(LT_car(packages))
        );
        packages = LT_cdr(packages);
    }
    if (packages != LT_NIL){
        LT_error("Package packagesAsList must return a proper list");
    }
}

static void LTREPL_completion_add_keywords(char* text){
    LTREPL_completion_add_package_symbols(text + 1, LT_PACKAGE_KEYWORD, ":", 1);
}

static char* LTREPL_completion_package_designator(char* text, char* colon){
    size_t length = (size_t)(colon - text);
    char* designator = malloc(length + 1);

    if (designator == NULL){
        LT_error("Could not allocate REPL completion package designator");
    }
    memcpy(designator, text, length);
    designator[length] = '\0';
    return designator;
}

static void LTREPL_completion_add_qualified_symbols(char* text, char* colon){
    char* package_designator = LTREPL_completion_package_designator(text, colon);
    LT_Package* package = LT_Package_resolve_used_package(
        LT_get_current_package(),
        package_designator
    );

    if (package == NULL){
        package = LT_Package_find(package_designator);
    }
    if (package != NULL){
        char* package_prefix = LT_sprintf("%s:", package_designator);

        LTREPL_completion_add_package_symbols(
            colon + 1,
            package,
            package_prefix,
            0
        );
    }
    free(package_designator);
}

static void LTREPL_completion_build_matches(char* text){
    char* colon = strrchr(text, ':');

    LTREPL_completion_matches_clear();
    if (text[0] == ':'){
        LTREPL_completion_add_keywords(text);
    } else if (colon != NULL){
        LTREPL_completion_add_qualified_symbols(text, colon);
    } else {
        LTREPL_completion_add_packages(text);
        LTREPL_completion_add_accessible_symbols(text);
    }
}

static char* LTREPL_completion_generator(const char* text, int state){
    if (state == 0){
        LTREPL_completion_build_matches((char*)text);
    }

    if (LTREPL_completion_matches.index >= LTREPL_completion_matches.count){
        return NULL;
    }
    return strdup(
        LTREPL_completion_matches.values[LTREPL_completion_matches.index++]
    );
}

static int LTREPL_completion_token_char_p(int ch){
    return !LTREPL_completion_reader_delimiter_p(ch);
}

static char** LTREPL_attempted_completion(const char* text, int start, int end){
    (void)text;
    (void)end;

    if (start > 0 && LTREPL_completion_token_char_p(rl_line_buffer[start - 1])){
        return NULL;
    }
    rl_attempted_completion_over = 1;
    return rl_completion_matches(
        rl_line_buffer + start,
        LTREPL_completion_generator
    );
}

static void LTREPL_install_completion(void){
    rl_attempted_completion_function = LTREPL_attempted_completion;
    rl_completer_word_break_characters = " \t\n()[]{}'`\",;";
    rl_completion_append_character = '\0';
}
#endif

void LTREPL_init(void){
#ifdef LT_HAVE_LIBEDIT
    LTREPL_install_completion();
#endif
    LTREPL_error_tag = LT_Symbol_new("repl-error");
}

LT_Value LTREPL_errorTag(void){
    return LTREPL_error_tag;
}

void LTREPL_printCondition(LT_Value condition){
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

LT_Value LTREPL_replErrorHandler(
    LT_Value arguments,
    LT_Value invocation_context_kind,
    LT_Value invocation_context_data,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
){
    LT_Value cursor = arguments;
    LT_Value condition;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, condition);
    LT_ARG_END(cursor);

    if (!LT_IncompleteInputSyntaxError_p(condition)){
        LTREPL_printCondition(condition);
    }

    LT_throw(LTREPL_error_tag, condition);
}

LT_Value LTREPL_scriptErrorHandler(
    LT_Value arguments,
    LT_Value invocation_context_kind,
    LT_Value invocation_context_data,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
){
    LT_Value cursor = arguments;
    LT_Value condition;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, condition);
    LT_ARG_END(cursor);

    LTREPL_printCondition(condition);

    LT_throw(LTREPL_error_tag, condition);
}

static int LTREPL_source_has_next_form(const char* source, size_t* offset){
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

int LTREPL_evalSourceString(const char* source,
                            LT_Value error_handler,
                            LT_Environment* environment,
                            int print_result,
                            LT_Value* caught_condition){
    size_t offset = 0;

    *caught_condition = LT_NIL;

    while (LTREPL_source_has_next_form(source, &offset)){
        LT_Reader* reader = LT_Reader_new(LT_NIL);
        LT_ReaderStream* stream = LT_ReaderStream_newForString(source + offset);
        LT_Value object = LT_NIL;
        LT_Value result = LT_NIL;

        LT_CATCH(LTREPL_error_tag, *caught_condition, {
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

static char* LTREPL_readLine(const char* prompt){
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

static void LTREPL_addHistoryEntry(const char* entry){
#ifdef LT_HAVE_LIBEDIT
    if (entry[0] != '\0'){
        add_history(entry);
    }
#else
    (void)entry;
#endif
}

int LTREPL_interact(LTREPL_Interaction* interaction){
    LT_StringBuilder* buffer = LT_StringBuilder_new();
    char* line;
    const char* primary_prompt;
    const char* continuation_prompt;

    if (interaction == NULL || interaction->evaluate == NULL){
        LT_error("REPL interaction requires an evaluator");
    }
    primary_prompt = interaction->primary_prompt == NULL
        ? ""
        : interaction->primary_prompt;
    continuation_prompt = interaction->continuation_prompt == NULL
        ? primary_prompt
        : interaction->continuation_prompt;

    while ((line = LTREPL_readLine(
        LT_StringBuilder_length(buffer) == 0
            ? primary_prompt
            : continuation_prompt
    )) != NULL){
        LT_Value caught = LT_NIL;
        int failed;
        size_t length = strlen(line);

        LT_StringBuilder_append_str(buffer, line);
        if (length == 0 || line[length - 1] != '\n'){
            LT_StringBuilder_append_char(buffer, '\n');
        }

        free(line);

        failed = interaction->evaluate(
            LT_StringBuilder_value(buffer),
            interaction->baton,
            &caught
        );
        if (failed && LT_IncompleteInputSyntaxError_p(caught)){
            continue;
        }

        LTREPL_addHistoryEntry(LT_StringBuilder_value(buffer));

        buffer = LT_StringBuilder_new();
    }

    puts("");

    if (LT_StringBuilder_length(buffer) > 0){
        LT_Value caught = LT_NIL;
        LTREPL_EvaluateFunction evaluate_at_eof = interaction->evaluate_at_eof
            ? interaction->evaluate_at_eof
            : interaction->evaluate;
        int failed = evaluate_at_eof(
            LT_StringBuilder_value(buffer),
            interaction->baton,
            &caught
        );

        if (failed){
            return 1;
        }
    }

    return 0;
}

typedef struct {
    LT_Value error_handler;
    LT_Value eof_error_handler;
    LT_Environment* environment;
} LTREPL_EvalBaton;

static int LTREPL_eval_interaction_evaluate(const char* source,
                                            void* baton_value,
                                            LT_Value* caught_condition){
    LTREPL_EvalBaton* baton = (LTREPL_EvalBaton*)baton_value;

    return LTREPL_evalSourceString(
        source,
        baton->error_handler,
        baton->environment,
        1,
        caught_condition
    );
}

static int LTREPL_eval_interaction_evaluate_at_eof(
    const char* source,
    void* baton_value,
    LT_Value* caught_condition
){
    LTREPL_EvalBaton* baton = (LTREPL_EvalBaton*)baton_value;

    return LTREPL_evalSourceString(
        source,
        baton->eof_error_handler,
        baton->environment,
        1,
        caught_condition
    );
}

int LTREPL_eval(LT_Value error_handler,
                LT_Value eof_error_handler,
                LT_Environment* environment){
    LTREPL_EvalBaton baton = {
        .error_handler = error_handler,
        .eof_error_handler = eof_error_handler,
        .environment = environment,
    };
    LTREPL_Interaction interaction = {
        .primary_prompt = "listtalk> ",
        .continuation_prompt = "      -> ",
        .evaluate = LTREPL_eval_interaction_evaluate,
        .evaluate_at_eof = LTREPL_eval_interaction_evaluate_at_eof,
        .baton = &baton,
    };

    return LTREPL_interact(&interaction);
}
