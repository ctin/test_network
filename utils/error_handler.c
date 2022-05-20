#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#ifndef TEMP_BUFFER_SIZE
#define TEMP_BUFFER_SIZE 1024
#endif //TEMP_BUFFER_SIZE

void FatalError(const char* message, ...) {
    char output[TEMP_BUFFER_SIZE] = {0};
    va_list argptr;
    va_start(argptr, message);
    vsnprintf (output, TEMP_BUFFER_SIZE - 1, message, argptr);
    printf("%s\n", output);
    va_end(argptr);
    exit(EXIT_FAILURE);
}