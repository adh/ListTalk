/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Printer.h>
#include <ListTalk/classes/Reader.h>
#include <ctype.h>
#include <stdio.h>

/* this does not belong here but in the actual reader implementation,
 * but implementation in reader would require working condition system, 
 * so here it is for now */
static int stream_has_next_form(FILE* stream){
    int ch = fgetc(stream);

    while (1){
        while (ch != EOF && isspace((unsigned char)ch)){
            ch = fgetc(stream);
        }

        if (ch == ';'){
            while (ch != EOF && ch != '\n'){
                ch = fgetc(stream);
            }
            ch = fgetc(stream);
            continue;
        }

        if (ch == EOF){
            return 0;
        }

        ungetc(ch, stream);
        return 1;
    }
}


int main(int argc, char**argv){
    LT_Reader* reader;
    LT_ReaderStream* stream;

    (void)argc;
    (void)argv;
    LT_init();
    reader = LT_Reader_new();
    stream = LT_ReaderStream_newForFile(stdin);

    while (stream_has_next_form(stdin)){
        LT_Value object;

        object = LT_Reader_readObject(reader, stream);
        LT_printer_print_object(object);
        fputc('\n', stdout);
    }

    return 0;
}
