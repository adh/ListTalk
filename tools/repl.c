#include <ListTalk/ListTalk.h>
#include <ListTalk/reader.h>
#include <ListTalk/printer.h>
#include <stdio.h>


int main(int argc, char**argv){
    LT_init();
    for(;;){
        LT_Object* expr = LT_reader_read_object();
        LT_Object* res = LT_eval(expr);
        LT_printer_print_object(res);
    }
    return 0;
}