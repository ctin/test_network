#include <stdio.h>

#include "event_loop/event_loop.h"
#include "async_server/async_server.h"
#include "server_handler/server_handler.h"
#include "utils/error_handler.h"

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

void* ServerThread(void* arg) {

    ServerStream *s;
    ServerInit();

    s = ServerNewStream();
    ServerAddListener(s, SERVER_EVENT_ERROR,  OnError);
    ServerAddListener(s, SERVER_EVENT_ACCEPT, OnAccept);

    int port = *(int*)arg;
    printf("starting server\n");
    ServerListen(s, port);
    printf("server started\n");
    while (ServerGetStreamsCount() > 0) {
        ServerUpdate();
        usleep(10 * 1000);
    }
    printf("server stopped\n");
    return NULL;
}

void StopServer() {
    ServerCloseAllStreams();
}

void SigIntHandler(int dummy) {
    printf("stopping server...\n");
    StopServer();
    printf("stopping loop...\n");
    StopLoop();
}

int main(int argc, char* argv[])
{
    if(argc < 2) {
        FatalError("please provide port\n");
    }

    char* str_end = NULL;
    uint16_t port = strtol(argv[1], (void*)str_end, 10);

    signal(SIGINT, SigIntHandler);

    RunLoop();

    pthread_t worker;
    int create_result = pthread_create(&worker, NULL, ServerThread, &port);
    if(create_result != 0) {
        FatalError("failed to run server thread: %d\n", create_result);
    }
    int join_result = pthread_join(worker, NULL);
    if(join_result != 0) {
        FatalError("failed to join server thread: %d\n", join_result);
    }

    return 0;
}



