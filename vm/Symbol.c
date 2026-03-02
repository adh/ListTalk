#include <ListTalk/Symbol.h>
#include <ListTalk/Class.h>

#include <ListTalk/utils.h>

struct LT_Symbol_s {
    LT_Object base;
    char* name;
};

static void Symbol_debugPrintOn(LT_Value obj, FILE* stream){
    fputs(LT_Symbol_name((LT_Symbol*)LT_VALUE_POINTER_VALUE(obj)), stream);
}

LT_DEFINE_CLASS(LT_Symbol) {
    .superclass = &LT_Class_class,
    .metaclass_superclass = &LT_Class_class,
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


LT_Symbol* LT_Symbol_new(char *name)
{
    LT_InlineHash* symbol_table = get_symbol_table();
    LT_Symbol* symbol = LT_StringHash_at(symbol_table, name);
    if (symbol != NULL){
        return symbol;
    }

    symbol = LT_Class_ALLOC(LT_Symbol);
    symbol->name = LT_strdup(name);
    LT_StringHash_at_put(symbol_table, symbol->name, symbol);
    return symbol;
}

char *LT_Symbol_name(LT_Symbol * symbol)
{
    return symbol->name;
}
