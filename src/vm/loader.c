/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/loader.h>

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int stream_has_next_form(LT_ReaderStream* stream){
    int ch = LT_ReaderStream_getc(stream);

    while (1){
        while (ch != EOF && isspace((unsigned char)ch)){
            ch = LT_ReaderStream_getc(stream);
        }

        if (ch == ';'){
            while (ch != EOF && ch != '\n'){
                ch = LT_ReaderStream_getc(stream);
            }
            ch = LT_ReaderStream_getc(stream);
            continue;
        }

        if (ch == EOF){
            return 0;
        }

        LT_ReaderStream_ungetc(stream, ch);
        return 1;
    }
}

static char* module_name_from_designator(LT_Value designator){
    if (LT_Symbol_p(designator)){
        return LT_Symbol_name(LT_Symbol_from_value(designator));
    }
    if (LT_String_p(designator)){
        return (char*)LT_String_value_cstr(LT_String_from_value(designator));
    }
    LT_error("Module designator must be symbol or string");
    return NULL;
}

static char* path_for_module_in_directory(char* directory, char* module_name){
    LT_StringBuilder* builder = LT_StringBuilder_new();
    size_t directory_length = strlen(directory);

    LT_StringBuilder_append_str(builder, directory);
    if (directory_length > 0 && directory[directory_length - 1] != '/'){
        LT_StringBuilder_append_char(builder, '/');
    }
    LT_StringBuilder_append_str(builder, module_name);
    LT_StringBuilder_append_str(builder, ".lt");
    return LT_StringBuilder_value(builder);
}

int LT_loader_load_file(char* path,
                        LT_Environment* target_environment,
                        LT_Value* result_out){
    FILE* file = fopen(path, "r");
    LT_Reader* reader;
    LT_ReaderStream* stream;
    LT_Value result = LT_NIL;

    if (file == NULL){
        return 0;
    }
    if (target_environment == NULL){
        LT_error("Loader expects environment");
    }

    reader = LT_Reader_new();
    stream = LT_ReaderStream_newForFile(file);
    while (stream_has_next_form(stream)){
        result = LT_eval(
            LT_Reader_readObject(reader, stream),
            target_environment,
            NULL
        );
    }
    fclose(file);

    if (result_out != NULL){
        *result_out = result;
    }
    return 1;
}

static int string_resolver_load(LT_Value resolver,
                                char* module_name,
                                LT_Environment* target_environment,
                                LT_Value* result_out){
    char* directory;
    char* path;

    if (!LT_String_p(resolver)){
        return 0;
    }

    directory = (char*)LT_String_value_cstr(LT_String_from_value(resolver));
    path = path_for_module_in_directory(directory, module_name);
    return LT_loader_load_file(path, target_environment, result_out);
}

static int resolver_load(LT_Value resolver,
                         char* module_name,
                         LT_Environment* target_environment,
                         LT_Value* result_out){
    if (LT_String_p(resolver)){
        return string_resolver_load(
            resolver,
            module_name,
            target_environment,
            result_out
        );
    }

    LT_error("Unsupported module resolver");
    return 0;
}

static LT_Value load_from_resolvers(LT_Environment* target_environment,
                                    char* module_name,
                                    LT_Value resolvers){
    LT_Value cursor = resolvers;

    while (cursor != LT_NIL){
        LT_Value resolver;
        LT_Value result = LT_NIL;

        if (!LT_Pair_p(cursor)){
            LT_error("Module resolver list must be proper");
        }

        resolver = LT_car(cursor);
        if (resolver_load(resolver, module_name, target_environment, &result)){
            return result;
        }

        cursor = LT_cdr(cursor);
    }

    LT_error("Unable to load module");
    return LT_NIL;
}

LT_Value LT_loader_load(LT_Environment* target_environment,
                        LT_Value module_designator,
                        LT_Value resolvers){
    char* module_name;

    if (target_environment == NULL){
        LT_error("Loader expects environment");
    }
    module_name = module_name_from_designator(module_designator);

    if (LT_String_p(resolvers)){
        LT_Value result = LT_NIL;

        if (!string_resolver_load(
            resolvers,
            module_name,
            target_environment,
            &result
        )){
            LT_error("Unable to load module");
        }
        return result;
    }

    return load_from_resolvers(target_environment, module_name, resolvers);
}
