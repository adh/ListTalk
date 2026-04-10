/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/cmdopts.h>

#include <stdio.h>
#include <string.h>

static int fail(const char* message){
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

static int expect(int condition, const char* message){
    if (!condition){
        return fail(message);
    }
    return 0;
}

static void count_argument(LT_CmdOpts* parser, void* baton, char* value){
    int* count = baton;
    (void)parser;
    (void)value;

    (*count)++;
}

static int test_options_and_arguments(void){
    LT_CmdOpts* parser = LT_CmdOpts_new(0);
    char* name = NULL;
    long count = 0;
    double threshold = 0.0;
    int verbose = 0;
    int arg_count = 0;
    char* argv[] = {
        "-vv",
        "--name=sample",
        "-c",
        "42",
        "--threshold",
        "2.5",
        "first",
        "second",
    };

    LT_CmdOpts_addFlagIncrement(parser, 'v', "verbose", 1, &verbose);
    LT_CmdOpts_addStringOption(parser, 'n', "name", &name);
    LT_CmdOpts_addLongOption(parser, 'c', "count", &count);
    LT_CmdOpts_addDoubleOption(parser, 't', "threshold", &threshold);
    LT_CmdOpts_addArgument(
        parser,
        LT_CMDOPTS_ARGUMENT_MULTIPLE,
        count_argument,
        &arg_count
    );

    LT_CmdOpts_parseArgv(parser, 8, argv);

    if (expect(verbose == 2, "clustered short flags increment")){
        return 1;
    }
    if (expect(name != NULL && strcmp(name, "sample") == 0, "long string option")){
        return 1;
    }
    if (expect(count == 42, "short long-valued option")){
        return 1;
    }
    if (expect(threshold == 2.5, "long double option with following argument")){
        return 1;
    }
    return expect(arg_count == 2, "multiple positional arguments");
}

static int test_strict_order(void){
    LT_CmdOpts* parser = LT_CmdOpts_new(LT_CMDOPTS_STRICT_ORDER);
    int verbose = 0;
    int arg_count = 0;
    char* argv[] = {
        "-v",
        "script.lt",
        "-not-an-option",
    };

    LT_CmdOpts_addFlagIncrement(parser, 'v', "verbose", 1, &verbose);
    LT_CmdOpts_addArgument(
        parser,
        LT_CMDOPTS_ARGUMENT_MULTIPLE,
        count_argument,
        &arg_count
    );

    LT_CmdOpts_parseArgv(parser, 3, argv);

    if (expect(verbose == 1, "strict order parses leading options")){
        return 1;
    }
    return expect(arg_count == 2, "strict order leaves later dashes positional");
}

static int test_argv_to_list(void){
    char* argv[] = {"one", "two"};
    LT_Value list = LT_CmdOpts_argvToList(2, argv);

    if (expect(LT_Pair_p(list), "argv list has first pair")){
        return 1;
    }
    if (expect(
        strcmp(LT_String_value_cstr(LT_String_from_value(LT_car(list))), "one") == 0,
        "argv list first value"
    )){
        return 1;
    }

    list = LT_cdr(list);
    if (expect(LT_Pair_p(list), "argv list has second pair")){
        return 1;
    }
    if (expect(
        strcmp(LT_String_value_cstr(LT_String_from_value(LT_car(list))), "two") == 0,
        "argv list second value"
    )){
        return 1;
    }

    return expect(LT_cdr(list) == LT_NIL, "argv list terminates");
}

static int test_single_argument_does_not_repeat(void){
    LT_CmdOpts* parser = LT_CmdOpts_new(0);
    char* value = NULL;
    char* argv[] = {"only"};

    LT_CmdOpts_addStringArgument(parser, 1, &value);
    LT_CmdOpts_parseArgv(parser, 1, argv);

    return expect(
        value != NULL && strcmp(value, "only") == 0,
        "single positional argument parses once"
    );
}

static int test_string_list_argument_collects_rest(void){
    LT_CmdOpts* parser = LT_CmdOpts_new(LT_CMDOPTS_STRICT_ORDER);
    int verbose = 0;
    char* script = NULL;
    LT_Value rest = LT_NIL;
    char* argv[] = {
        "-v",
        "script.lt",
        "--not-an-option",
        "arg",
    };

    LT_CmdOpts_addFlagIncrement(parser, 'v', "verbose", 1, &verbose);
    LT_CmdOpts_addStringArgument(parser, 0, &script);
    LT_CmdOpts_addStringListArgument(parser, 0, &rest);
    LT_CmdOpts_parseArgv(parser, 4, argv);

    if (expect(verbose == 1, "list argument preserves leading options")){
        return 1;
    }
    if (expect(script != NULL && strcmp(script, "script.lt") == 0, "list argument follows first positional")){
        return 1;
    }
    if (expect(LT_Pair_p(rest), "list argument has first rest pair")){
        return 1;
    }
    if (expect(
        strcmp(LT_String_value_cstr(LT_String_from_value(LT_car(rest))), "--not-an-option") == 0,
        "list argument first rest value"
    )){
        return 1;
    }

    rest = LT_cdr(rest);
    if (expect(LT_Pair_p(rest), "list argument has second rest pair")){
        return 1;
    }
    if (expect(
        strcmp(LT_String_value_cstr(LT_String_from_value(LT_car(rest))), "arg") == 0,
        "list argument second rest value"
    )){
        return 1;
    }

    return expect(LT_cdr(rest) == LT_NIL, "list argument terminates");
}

static int test_required_string_list_argument_accepts_one_value(void){
    LT_CmdOpts* parser = LT_CmdOpts_new(0);
    LT_Value rest = LT_NIL;
    char* argv[] = {"value"};

    LT_CmdOpts_addStringListArgument(parser, 1, &rest);
    LT_CmdOpts_parseArgv(parser, 1, argv);

    return expect(LT_Pair_p(rest), "required list argument accepts one value");
}

int main(void){
    int failures = 0;

    LT_init();

    failures += test_options_and_arguments();
    failures += test_strict_order();
    failures += test_argv_to_list();
    failures += test_single_argument_does_not_repeat();
    failures += test_string_list_argument_collects_rest();
    failures += test_required_string_list_argument_accepts_one_value();

    if (failures == 0){
        puts("cmdopts tests passed");
        return 0;
    }

    fprintf(stderr, "%d cmdopts test(s) failed\n", failures);
    return 1;
}
