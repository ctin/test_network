#include "server_handler/server_handler.h"
#include "async_server/async_server.h"
#include "event_loop/functions_queue.h"
#include "utils/error_handler.h"
#include "utils/realloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    ServerStream *stream;
    char data[PACKET_SIZE];
    int size;
} TestProjectTask;

void SendDataTask(void* data) {
    TestProjectTask* task = (TestProjectTask*)data;
    ServerWrite(task->stream, task->data, task->size);
}

void OnSendDataTaskFinished(void* data) {
    TestProjectTask* task = (TestProjectTask*)data;
    free(task);
}

void OnData(ServerEvent *e) {
    TestProjectTask* task = Realloc(NULL, sizeof(*task));
    task->stream = e->stream;
    task->size = e->size;
    // async server will own this data
    // and remove it after handler finishes
    memcpy(task->data, (const void*)e->data, e->size);
    AddTask(CreateTask(task, SendDataTask, OnSendDataTaskFinished));
}

void OnAccept(ServerEvent *e) {
    printf("OnAccept\n");
    ServerAddListener(e->remote, SERVER_EVENT_DATA, OnData);
    ServerWritef(e->remote, "echo server\r\n");
}

void OnError(ServerEvent *e) {
    FatalError("server error: %s\n", e->msg);
}
