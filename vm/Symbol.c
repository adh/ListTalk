#include <ListTalk/Symbol.h>

#include <ListTalk/utils.h>

struct LT_Symbol {
    LT_Object base;
    char* name;
};

LT_DEFINE_CLASS(LT_Symbol) {
    .name = "Symbol",
    .instance_size = sizeof(LT_Symbol)
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
    LT_Symbol* sym = LT_StringHash_at(get_symbol_table(), name);
    if (sym){
        return sym;
    }
    
    sym = LT_Class_ALLOC(LT_Symbol);
    sym->name = name;
    LT_StringHash_at_put(get_symbol_table(), name, sym);
    return sym;
}

char *LT_Symbol_name(LT_Symbol * symbol)
{
    return symbol->name;
}
