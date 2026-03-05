#include <ListTalk/classes/Pair.h>
#include <ListTalk/vm/Class.h>

struct LT_Pair_s {
    LT_Value car;
    LT_Value cdr;
};

static void Pair_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Pair* pair = LT_Pair_from_object(obj);

    fputs("(pair ", stream);
    if (LT_Value_class(pair->car)->debugPrintOn != NULL){
        LT_Value_class(pair->car)->debugPrintOn(pair->car, stream);
    } else {
        fputs("<value>", stream);
    }
    fputs(" . ", stream);
    if (LT_Value_class(pair->cdr)->debugPrintOn != NULL){
        LT_Value_class(pair->cdr)->debugPrintOn(pair->cdr, stream);
    } else {
        fputs("<value>", stream);
    }
    fputc(')', stream);
}

LT_DEFINE_CLASS(LT_Pair) {
    .superclass = &LT_Class_class,
    .metaclass_superclass = &LT_Class_class,
    .instance_size = sizeof(LT_Pair),
    .class_flags = LT_CLASS_FLAG_SPECIAL,
    .debugPrintOn = Pair_debugPrintOn,
};

LT_Value LT_cons(LT_Value car, LT_Value cdr){
    LT_Pair* pair = GC_NEW(LT_Pair);
    pair->car = car;
    pair->cdr = cdr;
    return ((LT_Value)(uintptr_t)pair) | LT_VALUE_POINTER_TAG_PAIR;
}

LT_Value LT_car(LT_Value pair){
    return LT_Pair_from_object(pair)->car;
}

LT_Value LT_cdr(LT_Value pair){
    return LT_Pair_from_object(pair)->cdr;
}

void LT_Pair_set_car(LT_Value pair, LT_Value value){
    LT_Pair_from_object(pair)->car = value;
}

void LT_Pair_set_cdr(LT_Value pair, LT_Value value){
    LT_Pair_from_object(pair)->cdr = value;
}
