/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/metadata.h>

#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/conditions.h>

static LT_Value metadata_reader_error_tag = LT_NIL;
static pthread_once_t metadata_reader_error_tag_once = PTHREAD_ONCE_INIT;

static void metadata_reader_error_tag_init(void)
{
    metadata_reader_error_tag =
        LT_Symbol_new_uninterned("metadata-reader-error");
}

static LT_Value metadata_reader_error_tag_value(void)
{
    pthread_once(
        &metadata_reader_error_tag_once,
        metadata_reader_error_tag_init
    );
    return metadata_reader_error_tag;
}

static LT_Value metadata_reader_error_handler_impl(
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
    (void)condition;
    LT_throw(metadata_reader_error_tag_value(), LT_TRUE);
}

static LT_Primitive metadata_reader_error_handler = {
    .function = metadata_reader_error_handler_impl,
    .flags = 0,
    .name = "metadata-reader-error-handler",
    .arguments = "(condition)",
    .description = "Catch reader errors while parsing metadata."
};

LT_Value LT_parse_lambda_list_metadata(char* arguments_text)
{
    LT_Value caught = LT_NIL;
    LT_Value parsed = LT_NIL;
    LT_Reader* reader;
    LT_ReaderStream* stream;

    if (arguments_text == NULL){
        return LT_NIL;
    }

    reader = LT_Reader_new(LT_NIL);
    stream = LT_ReaderStream_newForString(arguments_text);
    LT_CATCH(metadata_reader_error_tag_value(), caught, {
        LT_HANDLER_BIND(LT_Primitive_from_static(&metadata_reader_error_handler), {
            parsed = LT_Reader_readObject(reader, stream);
        });
    });

    if (caught != LT_NIL){
        return (LT_Value)(uintptr_t)LT_String_new_cstr(arguments_text);
    }
    return parsed;
}
