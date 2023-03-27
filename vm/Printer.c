#include <ListTalk/Printer.h>

struct LT_Printer {
    LT_Object base;

};

LT_DEFINE_CLASS(LT_Printer){
    .name = "Printer",
    .instance_size = sizeof(LT_Printer),
};

void LT_printer_print_object(LT_Object* object){
    
}
