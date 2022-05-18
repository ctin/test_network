#include "event_loop/event_loop.h"
#include "utils/error_handler.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

pthread_t g_worker;
bool g_should_run;

void* Loop(void* args) {

    while(g_should_run) {
        while(HasTasks()) {
            EventLoopFunction* func = PopTask();
            if(func == NULL) {
                FatalError("Internal error: has functions, but no function presented");
            } else {
                printf("invoking\n");
                func();
            }
        }
        usleep(10 * 1000);
    }
    return NULL;
}

void StopLoop() {
    g_should_run = false;
    int join_result = pthread_join(g_worker, NULL);
    if(join_result != 0) {
        FatalError("failed to join thread: %d", join_result);
    }
}

bool RunLoop() {
    g_should_run = true;

    int create_result = pthread_create(&g_worker, NULL, Loop, NULL);
    if(create_result != 0) {
        FatalError("failed to run threadЖ %d", create_result);
    }
    return true;
}
