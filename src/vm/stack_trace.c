/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/stack_trace.h>
#include <ListTalk/classes/ImmutableList.h>
#include <ListTalk/classes/StackFrame.h>
#include <ListTalk/classes/SourceLocation.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/vm/value.h>
#include <ListTalk/utils.h>

#include <stdio.h>

_Thread_local LT_StackFrame* LT__stack_trace_stack = NULL;

#define LT_STACK_TRACE_MAX_EXPANSION_DEPTH 256

static void stack_trace_print_indent(FILE* stream, unsigned int depth){
    unsigned int i;

    for (i = 0; i < depth; i++){
        fputs("  ", stream);
    }
}

static void stack_trace_print_source_location(FILE* stream,
                                              LT_Value expression,
                                              unsigned int indent_depth){
    LT_Value source_location;
    LT_Value source_file;

    if (!LT_ImmutableList_p(expression)){
        return;
    }

    source_location = LT_ImmutableList_source_location(expression);
    source_file = LT_ImmutableList_source_file(expression);

    if (source_location == LT_NIL && source_file == LT_NIL){
        return;
    }

    stack_trace_print_indent(stream, indent_depth);
    fputs("at ", stream);
    if (source_file != LT_NIL){
        if (LT_String_p(source_file)){
            fputs(
                LT_String_value_cstr(LT_String_from_value(source_file)),
                stream
            );
        } else {
            LT_Value_debugPrintOn(source_file, stream);
        }
    }
    if (source_location != LT_NIL){
        uint32_t line = LT_SourceLocation_line(source_location) + 1;
        uint32_t column = LT_SourceLocation_column(source_location) + 1;

        if (source_file != LT_NIL){
            fputc(':', stream);
        } else {
            fputs("line ", stream);
        }
        fprintf(stream, "%u", line);
        if (source_file == LT_NIL){
            fputs(", column ", stream);
        } else {
            fputc(':', stream);
        }
        fprintf(stream, "%u", column);
    }
    fputc('\n', stream);
}

static void stack_trace_print_expansion_chain(FILE* stream,
                                              LT_Value expression,
                                              unsigned int indent_depth){
    LT_Value cursor = expression;
    unsigned int depth = 0;

    while (LT_ImmutableList_p(cursor)){
        LT_Value original_expression = LT_ImmutableList_original_expression(cursor);

        if (original_expression == LT_NIL || original_expression == cursor){
            return;
        }

        stack_trace_print_indent(stream, indent_depth);
        fputs("expanded from ", stream);
        LT_Value_debugPrintOn(original_expression, stream);
        fputc('\n', stream);
        stack_trace_print_source_location(
            stream,
            original_expression,
            indent_depth + 1
        );

        cursor = original_expression;
        depth++;
        if (depth >= LT_STACK_TRACE_MAX_EXPANSION_DEPTH){
            stack_trace_print_indent(stream, indent_depth);
            fputs("expanded from ...\n", stream);
            return;
        }
    }
}

unsigned int LT_stack_trace_depth(void){
    unsigned int depth = 0;
    LT_StackFrame* frame = LT__stack_trace_stack;

    while (frame != NULL){
        depth += 1;
        frame = frame->previous;
    }

    return depth;
}

LT_Value LT_stack_trace_capture(void){
    LT_StackFrame* frame = LT__stack_trace_stack;
    LT_ListBuilder* builder = LT_ListBuilder_new();

    while (frame != NULL){
        LT_Value snapshot_frame = LT_NIL;

        if (frame->type == LT_STACK_FRAME_TYPE_EVAL){
            snapshot_frame = (LT_Value)(uintptr_t)LT_EvalStackFrame_new(
                frame->arguments.eval.expression,
                frame->arguments.eval.environment
            );
        } else if (frame->type == LT_STACK_FRAME_TYPE_APPLY){
            snapshot_frame = (LT_Value)(uintptr_t)LT_ApplyStackFrame_new(
                frame->arguments.apply.callable,
                frame->arguments.apply.arguments
            );
        }

        if (snapshot_frame != LT_NIL){
            LT_ListBuilder_append(builder, snapshot_frame);
        }
        frame = frame->previous;
    }

    return LT_ListBuilder_value(builder);
}

void LT_stack_trace_print(FILE* stream){
    LT_Value snapshot = LT_stack_trace_capture();
    LT_Value cursor = snapshot;
    unsigned int index = 0;

    if (snapshot == LT_NIL){
        return;
    }

    fputs("Backtrace:\n", stream);
    while (cursor != LT_NIL){
        LT_Value frame_object = LT_car(cursor);

        fprintf(stream, "  #%u ", index);
        if (LT_EvalStackFrame_p(frame_object)){
            LT_EvalStackFrame* frame = LT_EvalStackFrame_from_value(frame_object);
            fputs("eval expr=", stream);
            LT_Value_debugPrintOn(LT_EvalStackFrame_expression(frame), stream);
            fputc('\n', stream);
            stack_trace_print_source_location(
                stream,
                LT_EvalStackFrame_expression(frame),
                2
            );
            stack_trace_print_expansion_chain(
                stream,
                LT_EvalStackFrame_expression(frame),
                2
            );
        } else if (LT_ApplyStackFrame_p(frame_object)){
            LT_ApplyStackFrame* frame = LT_ApplyStackFrame_from_value(frame_object);
            fputs("apply callable=", stream);
            LT_Value_debugPrintOn(LT_ApplyStackFrame_callable(frame), stream);
            fputs(" args=", stream);
            LT_Value_debugPrintOn(LT_ApplyStackFrame_arguments(frame), stream);
            fputc('\n', stream);
        } else {
            fputs("unknown-frame\n", stream);
        }
        index += 1;
        cursor = LT_cdr(cursor);
    }
}
