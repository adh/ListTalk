/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Symbol.h>
#include <ListTalk/vm/Class.h>

#include <ListTalk/utils.h>

struct LT_Symbol_s {
    char* name;
};

static void Symbol_debugPrintOn(LT_Value obj, FILE* stream){
    fputs(LT_Symbol_name((LT_Symbol*)LT_VALUE_POINTER_VALUE(obj)), stream);
}

LT_DEFINE_CLASS(LT_Symbol) {
    .superclass = &LT_Class_class,
    .metaclass_superclass = &LT_Class_class,
    .class_flags = LT_CLASS_FLAG_SPECIAL,
    .instance_size = sizeof(LT_Symbol),
    .debugPrintOn = Symbol_debugPrintOn,
};


static LT_InlineHash* get_symbol_table(void){
    static LT_InlineHash* symbol_table = NULL;

    if (symbol_table == NULL){
        symbol_table = GC_NEW(LT_InlineHash);
        LT_InlineHash_init(symbol_table);
    }

    return symbol_table;
}


LT_Value LT_Symbol_new(char *name)
{
    LT_InlineHash* symbol_table = get_symbol_table();
    LT_Symbol* symbol = LT_StringHash_at(symbol_table, name);
    if (symbol != NULL){
        return ((LT_Value)(uintptr_t)symbol) | LT_VALUE_POINTER_TAG_SYMBOL;
    }

    symbol = GC_NEW(LT_Symbol);
    symbol->name = LT_strdup(name);
    LT_StringHash_at_put(symbol_table, symbol->name, symbol);
    return ((LT_Value)(uintptr_t)symbol) | LT_VALUE_POINTER_TAG_SYMBOL;
}

char *LT_Symbol_name(LT_Symbol * symbol)
{
    return symbol->name;
}
