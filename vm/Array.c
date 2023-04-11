#include <ListTalk/Array.h>

#include <string.h>
#include <stdio.h>
#include <assert.h>

struct LT_Array {
    LT_Object base;
    size_t length;
    LT_Object* items[];
};

LT_DEFINE_CLASS(LT_Array){
    .instance_size = sizeof(LT_Array),
};

struct LT_ImmutableTuple {
    LT_Object base;
    size_t length;
    LT_Object* items[];
};

static void ImmutableTuple_print(LT_Object* obj, FILE* stream){
    LT_ImmutableTuple* tuple = obj;
    fputs("(", stream);
    for (int i = 0; i < tuple->length; i++){
        if (i != 0){
            fputs(" ", stream);
        }
        LT_Object_printOn(tuple->items[i], stream);
    }
    fputs(")", stream);
}

LT_DEFINE_CLASS(LT_ImmutableTuple){
    .instance_size = sizeof(LT_ImmutableTuple),
    .printOn = ImmutableTuple_print,
};

size_t LT_ImmutableTuple_length(LT_ImmutableTuple* tuple){
    return tuple->length;
}

LT_Object* LT_ImmutableTuple_at(
    LT_ImmutableTuple* tuple,
    size_t position
){
    /* TODO: check bounds */

    return tuple->items[position];
}

struct LT_ImmutableTupleBuilder
{
    LT_Object base;
    size_t length;
    size_t size;
    LT_Object** buf;
};

LT_DEFINE_CLASS(LT_ImmutableTupleBuilder){
    .instance_size = sizeof(LT_ImmutableTupleBuilder),
};

LT_ImmutableTupleBuilder* LT_ImmutableTupleBuilder_new()
{
    LT_ImmutableTupleBuilder* builder = LT_Class_ALLOC(
        LT_ImmutableTupleBuilder
    );

    builder->size = 16;
    builder->length = 0;
    builder->buf = GC_MALLOC(sizeof(LT_Object*) * builder->size);

    return builder;
}

void LT_ImmutableTupleBuilder_add(
    LT_ImmutableTupleBuilder* builder, 
    LT_Object* object
){
    if (builder->size == builder->length){
        size_t new_size = builder->size * 2 + 2;
        LT_Object** new_buf = GC_MALLOC(sizeof(LT_Object*) * new_size);
        memcpy(new_buf, builder->buf, sizeof(LT_Object*) * builder->size);
        builder->buf = new_buf;
        builder->size = new_size;
    }
    builder->buf[builder->length] = object;
    builder->length++;
}

extern void LT_ImmutableTupleBuilder_atPut(
    LT_ImmutableTupleBuilder* builder, 
    size_t position,
    LT_Object* object
){
    assert(position < builder->length);
    builder->buf[position] = object;
}


LT_ImmutableTuple* LT_ImmutableTupleBuilder_value(
    LT_ImmutableTupleBuilder* builder
){
    LT_ImmutableTuple* tuple = LT_Class_ALLOC_FLEXIBLE(
        LT_ImmutableTuple, 
        sizeof(LT_Object*) * builder->length
    );
    tuple->length = builder->length;
    memcpy(tuple->items, builder->buf, sizeof(LT_Object*) * builder->length);
    return tuple;
}
