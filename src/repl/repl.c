/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/repl/repl.h>

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Condition.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/conditions.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/reader.h>
#include <ListTalk/vm/throw_catch.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#ifdef LT_HAVE_LIBEDIT
#ifdef LT_HAVE_EDITLINE_READLINE_H
#include <editline/readline.h>
#else
#include <readline/readline.h>
#endif
#endif

#define LT_REPL_DEFAULT_PROMPT "listtalk> "
#define LT_REPL_DEFAULT_CONTINUATION_INDENT 2

static LT_Value repl_reader_error_tag = LT_NIL;
static pthread_once_t repl_reader_error_tag_once = PTHREAD_ONCE_INIT;

struct LT_REPL_State_s {
    char* prompt;
    size_t continuation_indent;
    LT_StringBuilder* input;
    size_t offset;
    int eof;
    LT_REPL_Callback callback;
    void* callback_context;
};

static void repl_reader_error_tag_init(void){
    repl_reader_error_tag = LT_Symbol_new_uninterned("repl-reader-error");
}

static LT_Value repl_reader_error_tag_value(void){
    pthread_once(&repl_reader_error_tag_once, repl_reader_error_tag_init);
    return repl_reader_error_tag;
}

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

    if (current_package == NULL){
        return;
    }
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
    LT_Package* current_package = LT_get_current_package();
    LT_Package* package = current_package == NULL
        ? NULL
        : LT_Package_resolve_used_package(
            current_package,
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

static char* repl_completion_generator(const char* text, int completion_state){
    if (completion_state == 0){
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

static LT_Value repl_reader_error_handler_impl(
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

    LT_throw(repl_reader_error_tag_value(), condition);
}

static LT_Primitive repl_reader_error_handler = {
    .function = repl_reader_error_handler_impl,
    .flags = 0,
    .name = "repl-reader-error-handler",
    .arguments = "(condition)",
    .description = "Return reader errors to the REPL input state."
};

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

static char* repl_render_prompt(LT_REPL_State* state){
    LT_StringBuilder* rendered = LT_StringBuilder_new();
    const char* cursor = state->prompt;

    while (*cursor != '\0'){
        if (cursor[0] == '%' && cursor[1] == 'p'){
            LT_Package* package = LT_get_current_package();

            if (package != NULL){
                LT_StringBuilder_append_str(rendered, LT_Package_name(package));
            }
            cursor += 2;
        } else if (cursor[0] == '%' && cursor[1] == '%'){
            LT_StringBuilder_append_char(rendered, '%');
            cursor += 2;
        } else {
            LT_StringBuilder_append_char(rendered, *cursor++);
        }
    }
    return LT_StringBuilder_value(rendered);
}

static char* repl_continuation_prompt(LT_REPL_State* state){
    char* primary = repl_render_prompt(state);
    size_t primary_length = strlen(primary);
    size_t padding = primary_length > state->continuation_indent + 2
        ? primary_length - state->continuation_indent - 2
        : 0;
    LT_StringBuilder* prompt = LT_StringBuilder_new();

    while (padding-- > 0){
        LT_StringBuilder_append_char(prompt, ' ');
    }
    LT_StringBuilder_append_str(prompt, "-> ");
    return LT_StringBuilder_value(prompt);
}

static char* repl_read_line(const char* prompt){
#ifdef LT_HAVE_LIBEDIT
    return readline(prompt);
#else
    char* line = NULL;
    size_t capacity = 0;
    ssize_t length;
    (void)prompt;

    length = getline(&line, &capacity, stdin);
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

static void repl_reset_input(LT_REPL_State* state){
    state->input = LT_StringBuilder_new();
    state->offset = 0;
}

static int repl_append_line(LT_REPL_State* state, int continuation){
    char* prompt = continuation
        ? repl_continuation_prompt(state)
        : repl_render_prompt(state);
    char* line = repl_read_line(prompt);
    size_t length;

    if (line == NULL){
        state->eof = 1;
        puts(""); /* new line after ^D */
        return 0;
    }

    length = strlen(line);
    LT_StringBuilder_append_str(state->input, line);
    if (length == 0 || line[length - 1] != '\n'){
        LT_StringBuilder_append_char(state->input, '\n');
    }
    free(line);
    return 1;
}

LT_REPL_State* LT_REPL_State_new(void){
    LT_REPL_State* state = GC_NEW(LT_REPL_State);

    state->prompt = LT_strdup(LT_REPL_DEFAULT_PROMPT);
    state->continuation_indent = LT_REPL_DEFAULT_CONTINUATION_INDENT;
    state->eof = 0;
    repl_reset_input(state);
#ifdef LT_HAVE_LIBEDIT
    repl_install_completion();
#endif
    return state;
}

void LT_REPL_State_set_prompt(LT_REPL_State* state, const char* prompt){
    if (state == NULL || prompt == NULL){
        LT_error("REPL state and prompt must not be null");
    }
    state->prompt = LT_strdup((char*)prompt);
}

void LT_REPL_State_set_continuation_indent(
    LT_REPL_State* state,
    size_t indent
){
    if (state == NULL){
        LT_error("REPL state must not be null");
    }
    state->continuation_indent = indent;
}

LT_Value LT_REPL_State_read(LT_REPL_State* state){
    if (state == NULL){
        LT_error("REPL state must not be null");
    }

    while (1){
        LT_Value caught = LT_NIL;
        LT_Value object = LT_INVALID;
        LT_Reader* reader;
        LT_ReaderStream* stream;
        size_t object_offset;

        if (!source_has_next_form(
            LT_StringBuilder_value(state->input),
            &state->offset
        )){
            if (LT_StringBuilder_length(state->input) != 0){
                repl_add_history_entry(LT_StringBuilder_value(state->input));
                repl_reset_input(state);
            }
            if (state->eof || !repl_append_line(state, 0)){
                return LT_INVALID;
            }
            continue;
        }

        object_offset = state->offset;
        reader = LT_Reader_new(LT_NIL);
        stream = LT_ReaderStream_newForString(
            LT_StringBuilder_value(state->input) + object_offset
        );

        LT_CATCH(
            repl_reader_error_tag_value(),
            caught,
            {
                LT_HANDLER_BIND(
                    LT_Primitive_from_static(&repl_reader_error_handler),
                    {
                        object = LT_Reader_readObject(reader, stream);
                    }
                );
            }
        );

        if (caught != LT_NIL){
            state->offset = object_offset;
            if (LT_IncompleteInputSyntaxError_p(caught)
                && !state->eof
                && repl_append_line(state, 1)){
                continue;
            }

            repl_add_history_entry(LT_StringBuilder_value(state->input));
            repl_reset_input(state);
            LT_signal(caught);
            if (state->eof){
                return LT_INVALID;
            }
            continue;
        }

        state->offset = object_offset + LT_ReaderStream_stringOffset(stream);
        return object;
    }
}

void LT_REPL_State_loop(
    LT_REPL_State* state,
    LT_REPL_Callback callback,
    void* context
){
    LT_Value object;

    if (state == NULL || callback == NULL){
        LT_error("REPL state and callback must not be null");
    }

    state->callback = callback;
    state->callback_context = context;
    LT_UNWIND_PROTECT(
        {
            while ((object = LT_REPL_State_read(state)) != LT_INVALID){
                state->callback(object, state->callback_context);
            }
        },
        {
            state->callback = NULL;
            state->callback_context = NULL;
        }
    );

}
