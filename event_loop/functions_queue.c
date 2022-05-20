#include "event_loop/functions_queue.h"
#include "utils/error_handler.h"
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef QUEUE_SIZE
#error "QUEUE_SIZE already defined"
#endif //QUEUE_SIZE
#define QUEUE_SIZE 256

static EventLoopTaskData queue[QUEUE_SIZE] = {0};
static atomic_int g_current_position = 0;

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

EventLoopTaskData CreateTask(void* data, EventLoopTaskPtr task, EventLoopTaskFinishedCBPtr onTaskFinished) {
    EventLoopTaskData eventLoopTaskData;
    eventLoopTaskData.data = data;
    eventLoopTaskData.task = task;
    eventLoopTaskData.onTaskFinished = onTaskFinished;
    return eventLoopTaskData;
}

bool AddTask(EventLoopTaskData taskData) {
    if(g_current_position > QUEUE_SIZE) {
        return false;
    }
    LockMutex();

    queue[g_current_position] = taskData;
    ++g_current_position;

    UnlockMutex();
    return true;
}

bool HasTasks() {
    return g_current_position != 0;
}

EventLoopTaskData PopTask(bool* success) {
    if(g_current_position == 0) {
        *success = false;
    }
    *success = true;
    LockMutex();

    EventLoopTaskData task = queue[g_current_position - 1];

    --g_current_position;
    UnlockMutex();
    return task;
}