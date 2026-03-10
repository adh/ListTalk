/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/vm/throw_cacth.h>

#include <stdio.h>

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

static int test_catch_matching_tag_captures_value(void){
    LT_Value tag = LT_Symbol_new("tag-a");
    LT_Value caught = LT_FALSE;

    LT_CATCH(tag, caught, {
        LT_throw(tag, LT_SmallInteger_new(123));
    });

    return expect(
        LT_Value_is_fixnum(caught) && LT_SmallInteger_value(caught) == 123,
        "catch captures thrown value for matching tag"
    );
}

static int test_catch_uses_nearest_matching_frame(void){
    LT_Value outer_tag = LT_Symbol_new("tag-outer");
    LT_Value inner_tag = LT_Symbol_new("tag-inner");
    LT_Value outer_caught = LT_FALSE;
    LT_Value inner_caught = LT_FALSE;

    LT_CATCH(outer_tag, outer_caught, {
        LT_CATCH(inner_tag, inner_caught, {
            LT_throw(inner_tag, LT_SmallInteger_new(77));
        });
    });

    if (expect(
        LT_Value_is_fixnum(inner_caught) && LT_SmallInteger_value(inner_caught) == 77,
        "inner catch receives matching throw"
    )){
        return 1;
    }
    return expect(
        outer_caught == LT_FALSE,
        "outer catch is not triggered when inner catch matches"
    );
}

static int test_unwind_protect_runs_cleanup_on_normal_completion(void){
    int cleanup_runs = 0;
    int protected_ran = 0;

    LT_UNWIND_PROTECT(
    {
        protected_ran = 1;
    },
    {
        cleanup_runs += 1;
    });

    if (expect(protected_ran == 1, "protected body runs on normal path")){
        return 1;
    }
    return expect(cleanup_runs == 1, "cleanup runs once on normal path");
}

static int test_unwind_protect_runs_cleanup_and_rethrows(void){
    LT_Value tag = LT_Symbol_new("tag-up");
    LT_Value caught = LT_FALSE;
    int cleanup_runs = 0;

    LT_CATCH(tag, caught, {
        LT_UNWIND_PROTECT(
        {
            LT_throw(tag, LT_SmallInteger_new(99));
        },
        {
            cleanup_runs += 1;
        });
    });

    if (expect(cleanup_runs == 1, "cleanup runs during non-local exit")){
        return 1;
    }
    return expect(
        LT_Value_is_fixnum(caught) && LT_SmallInteger_value(caught) == 99,
        "unwind-protect rethrows original value"
    );
}

int main(void){
    int failures = 0;

    LT_init();

    failures += test_catch_matching_tag_captures_value();
    failures += test_catch_uses_nearest_matching_frame();
    failures += test_unwind_protect_runs_cleanup_on_normal_completion();
    failures += test_unwind_protect_runs_cleanup_and_rethrows();

    if (failures == 0){
        puts("throw/catch tests passed");
        return 0;
    }

    fprintf(stderr, "%d throw/catch test(s) failed\n", failures);
    return 1;
}
