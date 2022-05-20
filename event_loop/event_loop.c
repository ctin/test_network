#include "event_loop/event_loop.h"
#include "event_loop/functions_queue.h"
#include "utils/error_handler.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_t g_worker;
static bool g_should_run;

void* Loop(void* args) {
    printf("starting loop\n");
    while(g_should_run) {
        while(HasTasks()) {
            bool success;
            EventLoopTaskData taskData = PopTask(&success);
            if(success == false) {
                FatalError("Internal error: has functions, but no function presented");
            } else {
                printf("invoking task\n");
                taskData.task(taskData.data);
                taskData.onTaskFinished(taskData.data);
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

void RunLoop() {
    g_should_run = true;

    int create_result = pthread_create(&g_worker, NULL, Loop, NULL);
    if(create_result != 0) {
        FatalError("failed to run event loop thread: %d", create_result);
    }
    printf("Loop started");
}
