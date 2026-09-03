/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/repl/repl.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    LT_Value values[4];
    size_t count;
    LT_Value throw_tag;
} ReplResults;

static int fail(const char* message){
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

static void collect_object(LT_Value object, void* opaque){
    ReplResults* results = opaque;

    if (results->count < 4){
        results->values[results->count++] = object;
    }
}

static void collect_and_exit(LT_Value object, void* opaque){
    ReplResults* results = opaque;

    collect_object(object, opaque);
    LT_throw(results->throw_tag, object);
}

int main(void){
    static const char input[] = "1 2\n(+\n  3 4)\n";
    int input_pipe[2];
    LT_REPL_State* state;
    LT_Value first;
    LT_Value caught = LT_NIL;
    ReplResults results = {0};

    if (pipe(input_pipe) != 0){
        return fail("pipe failed");
    }
    if (write(input_pipe[1], input, strlen(input)) != (ssize_t)strlen(input)){
        return fail("write failed");
    }
    close(input_pipe[1]);
    if (dup2(input_pipe[0], STDIN_FILENO) < 0){
        return fail("dup2 failed");
    }
    close(input_pipe[0]);

    LT_INIT();
    LT_set_current_package(LT_PACKAGE_LISTTALK_USER);

    state = LT_REPL_State_new();
    LT_REPL_State_set_prompt(state, "[%p]> ");
    LT_REPL_State_set_continuation_indent(state, 4);

    first = LT_REPL_State_read(state);
    if (!LT_Value_is_fixnum(first) || LT_SmallInteger_value(first) != 1){
        return fail("read returns exactly the first object");
    }

    results.throw_tag = LT_Symbol_new_uninterned("repl-test-exit");
    LT_CATCH(results.throw_tag, caught, {
        LT_REPL_State_loop(state, collect_and_exit, &results);
    });
    if (!LT_Value_is_fixnum(caught) || LT_SmallInteger_value(caught) != 2){
        return fail("loop allows callback non-local exits");
    }

    LT_REPL_State_loop(state, collect_object, &results);
    if (results.count != 2){
        return fail("loop resumes with the next unread object");
    }
    if (!LT_Value_is_fixnum(results.values[0])
        || LT_SmallInteger_value(results.values[0]) != 2){
        return fail("loop preserves a second object from the same line");
    }
    if (!LT_Pair_p(results.values[1])){
        return fail("loop reads a multiline object");
    }

    puts("repl tests passed");
    return 0;
}
