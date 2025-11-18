#include <ListTalk/Symbol.h>

#include <ListTalk/utils.h>

struct LT_Symbol_s {
    LT_Object base;
    char* name;
};

static void Symbol_printOn(LT_Object* obj, FILE* stream){
    fputs(LT_Symbol_name(obj), stream);
}

LT_DEFINE_CLASS(LT_Symbol) {
    .name = "Symbol",
    .instance_size = sizeof(LT_Symbol),
    .printOn = Symbol_printOn,
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
    // TODO
}

char *LT_Symbol_name(LT_Symbol * symbol)
{
    return symbol->name;
}
