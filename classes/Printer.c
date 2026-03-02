#include <ListTalk/classes/Printer.h>
#include <ListTalk/vm/Class.h>
#include <stdio.h>

struct LT_Printer_s {
    LT_Object base;
};

LT_DEFINE_CLASS(LT_Printer) {
    .superclass = &LT_Class_class,
    .metaclass_superclass = &LT_Class_class,
    .instance_size = sizeof(LT_Printer),
};

void LT_printer_print_object(LT_Value object){
    LT_Class* klass = LT_Value_class(object);
    if (klass != NULL && klass->debugPrintOn != NULL){
        klass->debugPrintOn(object, stdout);
    }
}
