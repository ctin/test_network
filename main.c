#include <stdio.h>

#include "event_loop/event_loop.h"
#include "utils/error_handler.h"

#include <unistd.h>

void foo() {
    printf("test\n");
}

int main(int argc, char* argv[])
{
    AddTask(&foo);
    AddTask(&foo);
    RunLoop();
    usleep(1 * 100 * 1000);
    StopLoop();
    return 0;
}



