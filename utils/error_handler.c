#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void FatalError(const char* message, ...) {
    char output[1025];
    va_list argptr;
    va_start(argptr, message);
    vsnprintf (output, 1024, message, argptr);
    printf("%s\n", output);
    va_end(argptr);
    exit(EXIT_FAILURE);
}