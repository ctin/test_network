#pragma once

#include <stdbool.h>

typedef void (*EventLoopTaskPtr)(void*);
typedef void (*EventLoopTaskFinishedCBPtr)(void*);

typedef struct {
    // this structure should own the data
    // but only creator should manage that
    void* data;
    EventLoopTaskPtr task;
    EventLoopTaskFinishedCBPtr onTaskFinished;
} EventLoopTaskData;

EventLoopTaskData CreateTask(void* data, EventLoopTaskPtr task, EventLoopTaskFinishedCBPtr onTaskFinished);

bool AddTask(EventLoopTaskData taskData);
bool HasTasks();
EventLoopTaskData PopTask(bool* success);
