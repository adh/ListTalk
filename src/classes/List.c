/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/List.h>
#include <ListTalk/classes/ImmutableList.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>

static size_t list_length(LT_Value value){
    size_t length = 0;

    while (LT_Pair_p(value)){
        length++;
        value = LT_cdr(value);
    }

    if (value != LT_NIL){
        LT_error("List length requires proper list");
    }
    return length;
}

size_t LT_List_hash(LT_Value value){
    size_t hash = 0;

    while (LT_Pair_p(value)){
        size_t car_hash = LT_Value_hash(LT_car(value));

        hash = (hash * (size_t)33) ^ (car_hash + (size_t)0x9e3779b1);
        value = LT_cdr(value);
    }

    return (hash * (size_t)33) ^ (LT_Value_hash(value) + (size_t)0x9e3779b1);
}

int LT_List_equal_p(LT_Value left, LT_Value right){
    while (LT_Pair_p(left) && LT_Pair_p(right)){
        if (!LT_Value_equal_p(LT_car(left), LT_car(right))){
            return 0;
        }
        left = LT_cdr(left);
        right = LT_cdr(right);
    }

    if (LT_Pair_p(left) || LT_Pair_p(right)){
        return 0;
    }

    return LT_Value_equal_p(left, right);
}

void LT_List_debugPrintOn(LT_Value value, FILE* stream){
    fputc('(', stream);
    while (1){
        LT_Value_debugPrintOn(LT_car(value), stream);
        value = LT_cdr(value);

        if (value == LT_NIL){
            break;
        }
        if (LT_Pair_p(value)){
            fputc(' ', stream);
            continue;
        }

        fputs(" . ", stream);
        LT_Value_debugPrintOn(value, stream);
        break;
    }
    fputc(')', stream);
}

LT_Value LT_List_at(LT_Value list, size_t index){
    size_t current = 0;

    while (LT_Pair_p(list)){
        if (current == index){
            return LT_car(list);
        }
        current++;
        list = LT_cdr(list);
    }

    if (list != LT_NIL){
        LT_error("List index requires proper list");
    }
    LT_error("List index out of bounds");
    return LT_NIL;
}

LT_Value LT_List_map_many(LT_Value callable,
                          size_t list_count,
                          const LT_Value* lists){
    LT_ListBuilder* builder;
    LT_Value* cursors;
    size_t i;

    if (list_count == 0){
        LT_error("LT_List_map_many expects at least one list");
    }

    builder = LT_ListBuilder_new();
    cursors = GC_MALLOC(sizeof(LT_Value) * list_count);
    for (i = 0; i < list_count; i++){
        cursors[i] = lists[i];
    }

    while (1){
        LT_ListBuilder* argument_builder;

        for (i = 0; i < list_count; i++){
            if (cursors[i] == LT_NIL){
                return LT_ListBuilder_value(builder);
            }
            if (!LT_Pair_p(cursors[i])){
                LT_error("map expects proper lists");
            }
        }

        argument_builder = LT_ListBuilder_new();
        for (i = 0; i < list_count; i++){
            LT_ListBuilder_append(argument_builder, LT_car(cursors[i]));
            cursors[i] = LT_cdr(cursors[i]);
        }

        LT_ListBuilder_append(
            builder,
            LT_apply(callable, LT_ListBuilder_value(argument_builder), LT_NIL, LT_NIL, NULL)
        );
    }
}

LT_Value LT_List_map(LT_Value callable, LT_Value list){
    return LT_List_map_many(callable, 1, &list);
}

static LT_Value* list_cursors_new(size_t list_count, const LT_Value* lists){
    LT_Value* cursors;
    size_t i;

    if (list_count == 0){
        LT_error("List iteration expects at least one list");
    }

    cursors = GC_MALLOC(sizeof(LT_Value) * list_count);
    for (i = 0; i < list_count; i++){
        cursors[i] = lists[i];
    }
    return cursors;
}

static int list_cursors_collect_arguments(LT_Value* cursors,
                                          size_t list_count,
                                          LT_ListBuilder* argument_builder,
                                          const char* primitive_name){
    size_t i;

    for (i = 0; i < list_count; i++){
        if (cursors[i] == LT_NIL){
            return 0;
        }
        if (!LT_Pair_p(cursors[i])){
            LT_error("%s expects proper lists", primitive_name);
        }
    }

    for (i = 0; i < list_count; i++){
        LT_ListBuilder_append(argument_builder, LT_car(cursors[i]));
        cursors[i] = LT_cdr(cursors[i]);
    }
    return 1;
}

static void append_list_arguments(LT_ListBuilder* builder, LT_Value arguments){
    while (arguments != LT_NIL){
        LT_ListBuilder_append(builder, LT_car(arguments));
        arguments = LT_cdr(arguments);
    }
}

static LT_Value apply_left_fold_callable(LT_Value callable,
                                         LT_Value accumulator,
                                         LT_Value arguments){
    LT_ListBuilder* call_arguments = LT_ListBuilder_new();

    LT_ListBuilder_append(call_arguments, accumulator);
    append_list_arguments(call_arguments, arguments);
    return LT_apply(
        callable,
        LT_ListBuilder_value(call_arguments),
        LT_NIL,
        LT_NIL,
        NULL
    );
}

static LT_Value apply_right_fold_callable(LT_Value callable,
                                          LT_Value arguments,
                                          LT_Value accumulator){
    LT_ListBuilder* call_arguments = LT_ListBuilder_new();

    append_list_arguments(call_arguments, arguments);
    LT_ListBuilder_append(call_arguments, accumulator);
    return LT_apply(
        callable,
        LT_ListBuilder_value(call_arguments),
        LT_NIL,
        LT_NIL,
        NULL
    );
}

static LT_Value tuple_seed(LT_Value tuple, size_t list_count){
    if (list_count == 1){
        return LT_car(tuple);
    }
    return tuple;
}

static LT_Value collect_tuple_stack(size_t list_count,
                                    const LT_Value* lists,
                                    const char* primitive_name,
                                    size_t* tuple_count_out){
    LT_Value* cursors = list_cursors_new(list_count, lists);
    LT_Value stack = LT_NIL;
    size_t tuple_count = 0;

    while (1){
        LT_ListBuilder* argument_builder = LT_ListBuilder_new();

        if (!list_cursors_collect_arguments(
                cursors,
                list_count,
                argument_builder,
                primitive_name
            )){
            *tuple_count_out = tuple_count;
            return stack;
        }

        stack = LT_cons(LT_ListBuilder_value(argument_builder), stack);
        tuple_count++;
    }
}

void LT_List_for_each_many(LT_Value callable,
                           size_t list_count,
                           const LT_Value* lists){
    LT_Value* cursors = list_cursors_new(list_count, lists);

    while (1){
        LT_ListBuilder* argument_builder = LT_ListBuilder_new();

        if (!list_cursors_collect_arguments(
                cursors,
                list_count,
                argument_builder,
                "for-each"
            )){
            return;
        }

        (void)LT_apply(
            callable,
            LT_ListBuilder_value(argument_builder),
            LT_NIL,
            LT_NIL,
            NULL
        );
    }
}

void LT_List_for_each(LT_Value callable, LT_Value list){
    LT_List_for_each_many(callable, 1, &list);
}

LT_Value LT_List_any_many(LT_Value callable,
                          size_t list_count,
                          const LT_Value* lists){
    LT_Value* cursors = list_cursors_new(list_count, lists);

    while (1){
        LT_ListBuilder* argument_builder = LT_ListBuilder_new();
        LT_Value result;

        if (!list_cursors_collect_arguments(
                cursors,
                list_count,
                argument_builder,
                "any"
            )){
            return LT_FALSE;
        }

        result = LT_apply(
            callable,
            LT_ListBuilder_value(argument_builder),
            LT_NIL,
            LT_NIL,
            NULL
        );
        if (LT_Value_truthy_p(result)){
            return LT_TRUE;
        }
    }
}

LT_Value LT_List_any(LT_Value callable, LT_Value list){
    return LT_List_any_many(callable, 1, &list);
}

LT_Value LT_List_every_many(LT_Value callable,
                            size_t list_count,
                            const LT_Value* lists){
    LT_Value* cursors = list_cursors_new(list_count, lists);

    while (1){
        LT_ListBuilder* argument_builder = LT_ListBuilder_new();
        LT_Value result;

        if (!list_cursors_collect_arguments(
                cursors,
                list_count,
                argument_builder,
                "every"
            )){
            return LT_TRUE;
        }

        result = LT_apply(
            callable,
            LT_ListBuilder_value(argument_builder),
            LT_NIL,
            LT_NIL,
            NULL
        );
        if (!LT_Value_truthy_p(result)){
            return LT_FALSE;
        }
    }
}

LT_Value LT_List_every(LT_Value callable, LT_Value list){
    return LT_List_every_many(callable, 1, &list);
}

LT_Value LT_List_fold_left_many(LT_Value callable,
                                LT_Value initial,
                                size_t list_count,
                                const LT_Value* lists){
    LT_Value* cursors = list_cursors_new(list_count, lists);
    LT_Value accumulator = initial;

    while (1){
        LT_ListBuilder* argument_builder = LT_ListBuilder_new();

        if (!list_cursors_collect_arguments(
                cursors,
                list_count,
                argument_builder,
                "fold-left"
            )){
            return accumulator;
        }

        accumulator = apply_left_fold_callable(
            callable,
            accumulator,
            LT_ListBuilder_value(argument_builder)
        );
    }
}

LT_Value LT_List_fold_left(LT_Value callable, LT_Value initial, LT_Value list){
    return LT_List_fold_left_many(callable, initial, 1, &list);
}

LT_Value LT_List_fold_right_many(LT_Value callable,
                                 LT_Value initial,
                                 size_t list_count,
                                 const LT_Value* lists){
    size_t tuple_count;
    LT_Value stack = collect_tuple_stack(list_count, lists, "fold-right", &tuple_count);
    LT_Value accumulator = initial;
    (void)tuple_count;

    while (stack != LT_NIL){
        accumulator = apply_right_fold_callable(callable, LT_car(stack), accumulator);
        stack = LT_cdr(stack);
    }

    return accumulator;
}

LT_Value LT_List_fold_right(LT_Value callable, LT_Value initial, LT_Value list){
    return LT_List_fold_right_many(callable, initial, 1, &list);
}

LT_Value LT_List_reduce_left_many(LT_Value callable,
                                  size_t list_count,
                                  const LT_Value* lists){
    LT_Value* cursors = list_cursors_new(list_count, lists);
    LT_ListBuilder* argument_builder = LT_ListBuilder_new();
    LT_Value accumulator;

    if (!list_cursors_collect_arguments(
            cursors,
            list_count,
            argument_builder,
            "reduce-left"
        )){
        LT_error("reduce-left expects at least one element tuple");
    }

    accumulator = tuple_seed(LT_ListBuilder_value(argument_builder), list_count);

    while (1){
        argument_builder = LT_ListBuilder_new();
        if (!list_cursors_collect_arguments(
                cursors,
                list_count,
                argument_builder,
                "reduce-left"
            )){
            return accumulator;
        }

        accumulator = apply_left_fold_callable(
            callable,
            accumulator,
            LT_ListBuilder_value(argument_builder)
        );
    }
}

LT_Value LT_List_reduce_left(LT_Value callable, LT_Value list){
    return LT_List_reduce_left_many(callable, 1, &list);
}

LT_Value LT_List_reduce_right_many(LT_Value callable,
                                   size_t list_count,
                                   const LT_Value* lists){
    size_t tuple_count;
    LT_Value stack = collect_tuple_stack(list_count, lists, "reduce-right", &tuple_count);
    LT_Value accumulator;

    if (tuple_count == 0){
        LT_error("reduce-right expects at least one element tuple");
    }

    accumulator = tuple_seed(LT_car(stack), list_count);
    stack = LT_cdr(stack);

    while (stack != LT_NIL){
        accumulator = apply_right_fold_callable(callable, LT_car(stack), accumulator);
        stack = LT_cdr(stack);
    }

    return accumulator;
}

LT_Value LT_List_reduce_right(LT_Value callable, LT_Value list){
    return LT_List_reduce_right_many(callable, 1, &list);
}

LT_DEFINE_PRIMITIVE(
    list_method_length,
    "List>>length",
    "(self)",
    "Return list length."
){
    LT_Value cursor = arguments;
    LT_Value self;
    size_t length;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    length = list_length(self);
    return LT_Number_smallinteger_from_size(
        length,
        "List length does not fit fixnum"
    );
}

LT_DEFINE_PRIMITIVE(
    list_method_at,
    "List>>at:",
    "(self index)",
    "Return list item at index."
){
    LT_Value cursor = arguments;
    LT_Value self;
    int64_t index_value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_FIXNUM_ARG(cursor, index_value);
    LT_ARG_END(cursor);
    return LT_List_at(
        self,
        LT_Number_nonnegative_size_from_int64(
            index_value,
            "Negative index",
            "Index out of supported range"
        )
    );
}

LT_DEFINE_PRIMITIVE(
    list_method_map,
    "List>>map:",
    "(self callable)",
    "Return list of callable results for each element."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    return LT_List_map(callable, self);
}

LT_DEFINE_PRIMITIVE(
    list_method_for_each,
    "List>>for-each:",
    "(self callable)",
    "Apply callable to each element and return nil."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    LT_List_for_each(callable, self);
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE(
    list_method_any,
    "List>>any:",
    "(self callable)",
    "Return true when callable returns truthy for any element."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    return LT_List_any(callable, self);
}

LT_DEFINE_PRIMITIVE(
    list_method_every,
    "List>>every:",
    "(self callable)",
    "Return true when callable returns truthy for every element."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    return LT_List_every(callable, self);
}

LT_DEFINE_PRIMITIVE(
    list_method_reduce,
    "List>>reduce:",
    "(self callable)",
    "Reduce list from the left with callable."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    return LT_List_reduce_left(callable, self);
}

LT_DEFINE_PRIMITIVE(
    list_method_inject_into,
    "List>>inject:into:",
    "(self initial callable)",
    "Fold list from the left with initial value and callable."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value initial;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, initial);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    return LT_List_fold_left(callable, initial, self);
}

static LT_Method_Descriptor List_methods[] = {
    {"length", &list_method_length},
    {"at:", &list_method_at},
    {"map:", &list_method_map},
    {"for-each:", &list_method_for_each},
    {"any:", &list_method_any},
    {"every:", &list_method_every},
    {"reduce:", &list_method_reduce},
    {"inject:into:", &list_method_inject_into},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_List) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "List",
    .instance_size = 0,
    .class_flags = LT_CLASS_FLAG_SPECIAL | LT_CLASS_FLAG_ABSTRACT,
    .hash = LT_List_hash,
    .equal_p = LT_List_equal_p,
    .debugPrintOn = LT_List_debugPrintOn,
    .methods = List_methods,
};
