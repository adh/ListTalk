/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Dictionary.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/utils/ini.h>

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

static int test_lookup(void){
    const char* value = NULL;
    LT_INI* ini = LT_INI_parseBytes(
        "test.ini",
        "root = yes\n[database]\nhost = localhost\nport: 5432\n",
        strlen("root = yes\n[database]\nhost = localhost\nport: 5432\n"),
        LT_INI_ALLOW_GLOBAL_KEYS,
        LT_INI_DUPLICATE_LAST_WINS
    );

    if (expect(LT_INI_at(ini, NULL, "root", &value), "global lookup")){
        return 1;
    }
    if (expect(strcmp(value, "yes") == 0, "global value")){
        return 1;
    }
    if (expect(LT_INI_at(ini, "database", "host", &value), "section lookup")){
        return 1;
    }
    if (expect(strcmp(value, "localhost") == 0, "section value")){
        return 1;
    }
    return expect(!LT_INI_at(ini, "database", "missing", NULL), "missing key");
}

static int test_duplicate_last_wins(void){
    const char* value = NULL;
    LT_INI* ini = LT_INI_parseBytes(
        "test.ini",
        "[section]\nkey = old\nkey = new\n",
        strlen("[section]\nkey = old\nkey = new\n"),
        0,
        LT_INI_DUPLICATE_LAST_WINS
    );

    if (expect(LT_INI_at(ini, "section", "key", &value), "duplicate lookup")){
        return 1;
    }
    return expect(strcmp(value, "new") == 0, "last duplicate wins");
}

static int test_duplicate_first_wins(void){
    const char* value = NULL;
    LT_INI* ini = LT_INI_parseBytes(
        "test.ini",
        "[section]\nkey = old\nkey = new\n",
        strlen("[section]\nkey = old\nkey = new\n"),
        0,
        LT_INI_DUPLICATE_FIRST_WINS
    );

    if (expect(LT_INI_at(ini, "section", "key", &value), "duplicate lookup")){
        return 1;
    }
    return expect(strcmp(value, "old") == 0, "first duplicate wins");
}

static int test_dictionary_conversion(void){
    LT_INI* ini = LT_INI_parseBytes(
        "test.ini",
        "[section]\nkey = value\n",
        strlen("[section]\nkey = value\n"),
        0,
        LT_INI_DUPLICATE_LAST_WINS
    );
    LT_Dictionary* dictionary = LT_Dictionary_from_value(LT_INI_asDictionary(ini));
    LT_Value section_value = LT_NIL;
    LT_Value value = LT_NIL;

    if (expect(
        LT_Dictionary_at(
            dictionary,
            (LT_Value)(uintptr_t)LT_String_new_cstr("section"),
            &section_value
        ),
        "dictionary has section"
    )){
        return 1;
    }

    if (expect(
        LT_Dictionary_at(
            LT_Dictionary_from_value(section_value),
            (LT_Value)(uintptr_t)LT_String_new_cstr("key"),
            &value
        ),
        "section dictionary has key"
    )){
        return 1;
    }
    return expect(
        strcmp(LT_String_value_cstr(LT_String_from_value(value)), "value") == 0,
        "dictionary value"
    );
}

int main(void){
    int failures = 0;

    LT_INIT();

    failures += test_lookup();
    failures += test_duplicate_last_wins();
    failures += test_duplicate_first_wins();
    failures += test_dictionary_conversion();

    if (failures){
        return 1;
    }
    printf("ini tests passed\n");
    return 0;
}
