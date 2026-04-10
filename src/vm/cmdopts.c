/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/cmdopts.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

typedef struct LT_CmdOptsOption LT_CmdOptsOption;
typedef struct LT_CmdOptsArgument LT_CmdOptsArgument;

struct LT_CmdOptsOption {
    int has_argument;
    char short_option;
    char* long_option;
    LT_CmdOptsCallback callback;
    void* baton;
    LT_CmdOptsOption* next;
};

struct LT_CmdOptsArgument {
    int flags;
    size_t count;
    LT_CmdOptsCallback callback;
    void* baton;
    LT_CmdOptsArgument* next;
};

struct LT_CmdOpts {
    int flags;
    LT_CmdOptsOption* options;
    LT_CmdOptsOption* last_option;
    LT_CmdOptsArgument* arguments;
    LT_CmdOptsArgument* last_argument;
    LT_CmdOptsArgument* current_argument;
};

typedef struct {
    int argc;
    char** argv;
    int index;
} ArgvSource;

typedef struct {
    LT_Value cursor;
} ListSource;

typedef struct {
    char* pending;
    int has_pending;
    LT_CmdOptsSource source;
    void* baton;
} ParserSource;

typedef struct {
    char** place;
} StringBaton;

typedef struct {
    LT_Value* place;
    LT_ListBuilder* builder;
} StringListBaton;

typedef struct {
    long* place;
} LongBaton;

typedef struct {
    double* place;
} DoubleBaton;

typedef struct {
    int value;
    int* place;
} FlagBaton;

static char* next_arg(ParserSource* source){
    if (source->has_pending){
        source->has_pending = 0;
        return source->pending;
    }

    return source->source(source->baton);
}

static void push_arg(ParserSource* source, char* value){
    source->pending = value;
    source->has_pending = 1;
}

static LT_CmdOptsOption* find_short_option(LT_CmdOpts* parser, char name){
    LT_CmdOptsOption* option = parser->options;

    while (option != NULL){
        if (option->short_option == name){
            return option;
        }
        option = option->next;
    }

    return NULL;
}

static LT_CmdOptsOption* find_long_option(LT_CmdOpts* parser, char* name){
    LT_CmdOptsOption* option = parser->options;

    while (option != NULL){
        if (option->long_option != NULL
            && strcmp(option->long_option, name) == 0){
            return option;
        }
        option = option->next;
    }

    return NULL;
}

static void call_option(LT_CmdOpts* parser,
                        LT_CmdOptsOption* option,
                        char* value){
    if (option->callback != NULL){
        option->callback(parser, option->baton, value);
    }
}

static void call_argument(LT_CmdOpts* parser,
                          LT_CmdOptsArgument* argument,
                          char* value){
    if (argument->callback != NULL){
        argument->callback(parser, argument->baton, value);
    }
}

static LT_CmdOptsArgument* next_argument(LT_CmdOpts* parser){
    return parser->current_argument;
}

static void parse_argument(LT_CmdOpts* parser, char* value){
    LT_CmdOptsArgument* argument = next_argument(parser);

    if (argument == NULL){
        LT_error("Unexpected command line argument");
    }

    call_argument(parser, argument, value);
    argument->count++;

    if ((argument->flags & LT_CMDOPTS_ARGUMENT_MULTIPLE) == 0){
        parser->current_argument = argument->next;
    } else {
        parser->current_argument = argument;
    }
}

static void parse_long_option(LT_CmdOpts* parser,
                              ParserSource* source,
                              char* value){
    char* name = value + 2;
    char* delimiter = strchr(name, '=');
    char* argument = NULL;
    LT_CmdOptsOption* option;

    if (delimiter != NULL){
        size_t name_length = (size_t)(delimiter - name);
        char* name_copy = GC_MALLOC_ATOMIC(name_length + 1);

        memcpy(name_copy, name, name_length);
        name_copy[name_length] = '\0';
        name = name_copy;
        argument = delimiter + 1;
    }

    option = find_long_option(parser, name);
    if (option == NULL){
        LT_error("Unknown command line option");
    }

    if (option->has_argument){
        if (argument == NULL){
            argument = next_arg(source);
        }
        if (argument == NULL){
            LT_error("Missing command line option argument");
        }
    } else if (argument != NULL){
        LT_error("Unexpected command line option argument");
    }

    call_option(parser, option, argument);
}

static void parse_short_options(LT_CmdOpts* parser,
                                ParserSource* source,
                                char* value){
    char* cursor = value + 1;

    while (*cursor != '\0'){
        LT_CmdOptsOption* option = find_short_option(parser, *cursor);
        char* argument = NULL;

        if (option == NULL){
            LT_error("Unknown command line option");
        }

        cursor++;
        if (option->has_argument){
            if (*cursor != '\0'){
                argument = cursor;
            } else {
                argument = next_arg(source);
            }
            if (argument == NULL){
                LT_error("Missing command line option argument");
            }
            call_option(parser, option, argument);
            return;
        }

        call_option(parser, option, NULL);
    }
}

static void check_required_arguments(LT_CmdOpts* parser){
    LT_CmdOptsArgument* argument = parser->arguments;

    while (argument != NULL){
        if ((argument->flags & LT_CMDOPTS_ARGUMENT_REQUIRED) != 0
            && argument->count == 0){
            LT_error("Missing command line argument");
        }
        argument = argument->next;
    }
}

static void reset_arguments(LT_CmdOpts* parser){
    LT_CmdOptsArgument* argument = parser->arguments;

    while (argument != NULL){
        argument->count = 0;
        argument = argument->next;
    }
}

static void parse_rest_as_arguments(LT_CmdOpts* parser, ParserSource* source){
    char* value;

    while ((value = next_arg(source)) != NULL){
        parse_argument(parser, value);
    }
}

LT_CmdOpts* LT_CmdOpts_new(int flags){
    LT_CmdOpts* parser = GC_NEW(LT_CmdOpts);

    parser->flags = flags;
    return parser;
}

void LT_CmdOpts_addOption(LT_CmdOpts* parser,
                          int has_argument,
                          char short_option,
                          char* long_option,
                          LT_CmdOptsCallback callback,
                          void* baton){
    LT_CmdOptsOption* option = GC_NEW(LT_CmdOptsOption);

    option->has_argument = has_argument;
    option->short_option = short_option;
    option->long_option = long_option != NULL ? LT_strdup(long_option) : NULL;
    option->callback = callback;
    option->baton = baton;

    if (parser->last_option == NULL){
        parser->options = option;
    } else {
        parser->last_option->next = option;
    }
    parser->last_option = option;
}

void LT_CmdOpts_addArgument(LT_CmdOpts* parser,
                            int flags,
                            LT_CmdOptsCallback callback,
                            void* baton){
    LT_CmdOptsArgument* argument = GC_NEW(LT_CmdOptsArgument);

    argument->flags = flags;
    argument->callback = callback;
    argument->baton = baton;

    if (parser->last_argument == NULL){
        parser->arguments = argument;
    } else {
        parser->last_argument->next = argument;
    }
    parser->last_argument = argument;
}

void LT_CmdOpts_parse(LT_CmdOpts* parser,
                      LT_CmdOptsSource source,
                      void* baton){
    ParserSource parser_source = {
        .source = source,
        .baton = baton,
    };
    char* value;

    reset_arguments(parser);
    parser->current_argument = parser->arguments;

    while ((value = next_arg(&parser_source)) != NULL){
        if (strcmp(value, "--") == 0){
            parse_rest_as_arguments(parser, &parser_source);
            break;
        }

        if (value[0] == '-' && value[1] != '\0'){
            if (value[1] == '-'){
                parse_long_option(parser, &parser_source, value);
            } else {
                parse_short_options(parser, &parser_source, value);
            }
        } else {
            if ((parser->flags & LT_CMDOPTS_STRICT_ORDER) != 0){
                push_arg(&parser_source, value);
                parse_rest_as_arguments(parser, &parser_source);
                break;
            }
            parse_argument(parser, value);
        }
    }

    check_required_arguments(parser);
}

static char* argv_source(void* baton){
    ArgvSource* source = baton;

    if (source->index >= source->argc){
        return NULL;
    }

    return source->argv[source->index++];
}

void LT_CmdOpts_parseArgv(LT_CmdOpts* parser, int argc, char** argv){
    ArgvSource source = {
        .argc = argc,
        .argv = argv,
    };

    LT_CmdOpts_parse(parser, argv_source, &source);
}

static char* list_source(void* baton){
    ListSource* source = baton;
    LT_Value value;

    if (source->cursor == LT_NIL){
        return NULL;
    }
    if (!LT_Pair_p(source->cursor)){
        LT_error("Command line argument list is not a proper list");
    }

    value = LT_car(source->cursor);
    source->cursor = LT_cdr(source->cursor);

    if (!LT_String_p(value)){
        LT_error("Command line argument is not a string");
    }

    return (char*)LT_String_value_cstr(LT_String_from_value(value));
}

void LT_CmdOpts_parseList(LT_CmdOpts* parser, LT_Value list){
    ListSource source = {
        .cursor = list,
    };

    LT_CmdOpts_parse(parser, list_source, &source);
}

LT_Value LT_CmdOpts_argvToList(int argc, char** argv){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    int index;

    for (index = 0; index < argc; index++){
        LT_ListBuilder_append(
            builder,
            (LT_Value)(uintptr_t)LT_String_new_cstr(argv[index])
        );
    }

    return LT_ListBuilder_value(builder);
}

static void string_callback(LT_CmdOpts* parser, void* baton, char* value){
    StringBaton* string_baton = baton;
    (void)parser;

    *string_baton->place = value;
}

void LT_CmdOpts_addStringArgument(LT_CmdOpts* parser,
                                  int required,
                                  char** value){
    StringBaton* baton = GC_NEW(StringBaton);

    baton->place = value;
    LT_CmdOpts_addArgument(
        parser,
        required ? LT_CMDOPTS_ARGUMENT_REQUIRED : 0,
        string_callback,
        baton
    );
}

static void string_list_callback(LT_CmdOpts* parser, void* baton, char* value){
    StringListBaton* string_list_baton = baton;
    (void)parser;

    LT_ListBuilder_append(
        string_list_baton->builder,
        (LT_Value)(uintptr_t)LT_String_new_cstr(value)
    );
    *string_list_baton->place = LT_ListBuilder_value(string_list_baton->builder);
}

void LT_CmdOpts_addStringListArgument(LT_CmdOpts* parser,
                                      int required,
                                      LT_Value* value){
    StringListBaton* baton = GC_NEW(StringListBaton);

    *value = LT_NIL;
    baton->place = value;
    baton->builder = LT_ListBuilder_new();
    LT_CmdOpts_addArgument(
        parser,
        LT_CMDOPTS_ARGUMENT_MULTIPLE
            | (required ? LT_CMDOPTS_ARGUMENT_REQUIRED : 0),
        string_list_callback,
        baton
    );
}

void LT_CmdOpts_addStringOption(LT_CmdOpts* parser,
                                char short_option,
                                char* long_option,
                                char** value){
    StringBaton* baton = GC_NEW(StringBaton);

    baton->place = value;
    LT_CmdOpts_addOption(
        parser,
        1,
        short_option,
        long_option,
        string_callback,
        baton
    );
}

static void long_callback(LT_CmdOpts* parser, void* baton, char* value){
    LongBaton* long_baton = baton;
    char* end;
    long result;
    (void)parser;

    errno = 0;
    result = strtol(value, &end, 0);
    if (errno != 0 || end == value || *end != '\0'){
        LT_error("Invalid long command line argument");
    }

    *long_baton->place = result;
}

void LT_CmdOpts_addLongArgument(LT_CmdOpts* parser,
                                int required,
                                long* value){
    LongBaton* baton = GC_NEW(LongBaton);

    baton->place = value;
    LT_CmdOpts_addArgument(
        parser,
        required ? LT_CMDOPTS_ARGUMENT_REQUIRED : 0,
        long_callback,
        baton
    );
}

void LT_CmdOpts_addLongOption(LT_CmdOpts* parser,
                              char short_option,
                              char* long_option,
                              long* value){
    LongBaton* baton = GC_NEW(LongBaton);

    baton->place = value;
    LT_CmdOpts_addOption(
        parser,
        1,
        short_option,
        long_option,
        long_callback,
        baton
    );
}

static void double_callback(LT_CmdOpts* parser, void* baton, char* value){
    DoubleBaton* double_baton = baton;
    char* end;
    double result;
    (void)parser;

    errno = 0;
    result = strtod(value, &end);
    if (errno != 0 || end == value || *end != '\0'){
        LT_error("Invalid double command line argument");
    }

    *double_baton->place = result;
}

void LT_CmdOpts_addDoubleArgument(LT_CmdOpts* parser,
                                  int required,
                                  double* value){
    DoubleBaton* baton = GC_NEW(DoubleBaton);

    baton->place = value;
    LT_CmdOpts_addArgument(
        parser,
        required ? LT_CMDOPTS_ARGUMENT_REQUIRED : 0,
        double_callback,
        baton
    );
}

void LT_CmdOpts_addDoubleOption(LT_CmdOpts* parser,
                                char short_option,
                                char* long_option,
                                double* value){
    DoubleBaton* baton = GC_NEW(DoubleBaton);

    baton->place = value;
    LT_CmdOpts_addOption(
        parser,
        1,
        short_option,
        long_option,
        double_callback,
        baton
    );
}

static void flag_set_callback(LT_CmdOpts* parser, void* baton, char* value){
    FlagBaton* flag_baton = baton;
    (void)parser;
    (void)value;

    *flag_baton->place = flag_baton->value;
}

void LT_CmdOpts_addFlagSet(LT_CmdOpts* parser,
                           char short_option,
                           char* long_option,
                           int value,
                           int* place){
    FlagBaton* baton = GC_NEW(FlagBaton);

    baton->value = value;
    baton->place = place;
    LT_CmdOpts_addOption(
        parser,
        0,
        short_option,
        long_option,
        flag_set_callback,
        baton
    );
}

static void flag_increment_callback(LT_CmdOpts* parser,
                                    void* baton,
                                    char* value){
    FlagBaton* flag_baton = baton;
    (void)parser;
    (void)value;

    *flag_baton->place += flag_baton->value;
}

void LT_CmdOpts_addFlagIncrement(LT_CmdOpts* parser,
                                 char short_option,
                                 char* long_option,
                                 int value,
                                 int* place){
    FlagBaton* baton = GC_NEW(FlagBaton);

    baton->value = value;
    baton->place = place;
    LT_CmdOpts_addOption(
        parser,
        0,
        short_option,
        long_option,
        flag_increment_callback,
        baton
    );
}
