#include <ListTalk/error.h>

#include <stdio.h>
#include <stdlib.h>

void LT_error(const char* message, ...) {
    fprintf(stderr, "Error: %s\n", message);
    abort();
}
