#include "event_loop/functions_queue.h"
#include "utils/error_handler.h"
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>

pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

const int QUEUE_SIZE = 256;

EventLoopFunction *queue[QUEUE_SIZE] = {0};
atomic_int g_current_position = 0;

static void LockMutex() {
    int lock_result = pthread_mutex_lock(&g_mutex);
    if(lock_result != 0) {
        FatalError("failed to lock mutex: %d", lock_result);
    }
}

static void UnlockMutex() {
    int unlock_result = pthread_mutex_unlock(&g_mutex);
    if(unlock_result != 0) {
        FatalError("failed to lock mutex: %d", unlock_result);
    }
}

bool AddTask(EventLoopFunction func) {
    if(g_current_position > QUEUE_SIZE) {
        return false;
    }
    LockMutex();

    queue[g_current_position] = func;
    ++g_current_position;

    UnlockMutex();
    return true;
}

bool HasTasks() {
    return g_current_position != 0;
}

EventLoopFunction* PopTask() {
    if(g_current_position == 0) {
        return NULL;
    }
    LockMutex();

    EventLoopFunction* func = queue[g_current_position - 1];

    --g_current_position;
    UnlockMutex();
    return func;
}