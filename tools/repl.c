#include <ListTalk/ListTalk.h>
#include <ListTalk/Reader.h>
#include <ListTalk/Printer.h>
#include <stdio.h>


int main(int argc, char**argv){
    LT_init();
    LT_Reader* reader = LT_Reader_newForStream(stdin);
    for(;;){
        printf("]=> ");
        LT_Object* expr = LT_Reader_readObject(reader);
        LT_Object* res = LT_eval(expr);
        // LT_Object_printOn(res, stdout);
        printf("\n");
    }
    return 0;
}