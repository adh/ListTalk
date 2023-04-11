#include <ListTalk/ListTalk.h>
#include <ListTalk/Reader.h>
#include <ListTalk/Printer.h>
#include <stdio.h>


int main(int argc, char**argv){
    LT_init();
    for(;;){
        LT_Object* expr = LT_reader_read_object();
        LT_Object* res = LT_eval(expr);
        LT_Object_printOn(res, stdout);
    }
    return 0;
}