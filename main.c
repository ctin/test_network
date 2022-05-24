#include "utils/error_handler.h"
#include "utils/wierd_strrev.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <assert.h>
#include <errno.h>
#include <ev.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define assertm(exp, msg) assert(((void)msg, exp))

/* message length limitation */
#define MAX_MESSAGE_LEN (256)

EV_P;
struct ev_async asyncWatcher;

static int CreateServerFD(char const* addr, uint16_t u16port) {
  int fd;
  struct sockaddr_in server;

  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    FatalError("Failed to create socket");
  }

  server.sin_family = AF_INET;
  server.sin_port = htons(u16port);
  if (inet_pton(AF_INET, addr, &server.sin_addr) < 0) {
    FatalError("failed to call inet_pton: %s", strerror(errno));
  }

  if (bind(fd, (struct sockaddr*)&server, sizeof(server)) < 0) {
    FatalError("failed to call bind: %s", strerror(errno));
  }

  if (listen(fd, 10) < 0) {
    FatalError("failed to call listen: %s", strerror(errno));
  }

  return fd;
}

static void ReadCB(EV_P_ ev_io* watcher, int revents) {
  ssize_t ret;
  char buf[MAX_MESSAGE_LEN] = {0};

  ret = recv(watcher->fd, buf, sizeof(buf) - 1, MSG_DONTWAIT);
  if (ret > 0) {
    ;
    write(watcher->fd, strrev((char*)buf), ret);

  } else if ((ret < 0) && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    return;
  } else {
    fprintf(stdout, "client closed (fd=%d)\n", watcher->fd);
    ev_io_stop(EV_A_ watcher);
    close(watcher->fd);
    free(watcher);
  }
}

static void AcceptCB(EV_P_ ev_io* watcher, int revents) {
  int connfd;
  ev_io* client;

  connfd = accept(watcher->fd, NULL, NULL);
  if (connfd > 0) {
    client = calloc(1, sizeof(*client));
    ev_io_init(client, ReadCB, connfd, EV_READ);
    ev_io_start(EV_A_ client);
  } else if ((connfd < 0) && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    return;
  } else {
    close(watcher->fd);
    ev_break(EV_A_ EVBREAK_ALL);
    /* this will lead main to exit, no need to free watchers of clients */
  }
}

void AsyncStopCB(EV_P_ ev_async* async_watcher, int revents) {
  printf("sending break to the worker thread\n");
  ev_break(loop, EVBREAK_ONE);
}

static void StopServer() {
  printf("got sigint\n");
  ev_async_init(&asyncWatcher, AsyncStopCB);
  ev_async_start(loop, &asyncWatcher);
  ev_async_send(loop, &asyncWatcher);
}

static void* ServerThread(void* arg) {
  ev_io* watcher;

  int fd = *(int*)arg;

  loop = ev_default_loop(EVFLAG_NOENV);
  watcher = calloc(1, sizeof(*watcher));
  assertm(loop && watcher, "can not alloc memory\n");

  /* set nonblock flag */

  ev_io_init(watcher, AcceptCB, fd, EV_READ);
  ev_io_start(EV_A_ watcher);

  ev_signal exitsig;
  ev_signal_init(&exitsig, StopServer, SIGINT);
  ev_signal_start(loop, &exitsig);

  ev_run(EV_A_ 0);

  ev_loop_destroy(EV_A);
  free(watcher);
  return NULL;
}

static int CreateServer(char const* addr, uint16_t u16port) {
  int fd = CreateServerFD(addr, u16port);
  if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) < 0) {
    FatalError("failed to call fcntl: %s", strerror(errno));
  }
  return fd;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    FatalError("please provide port\n");
  }
  uint16_t port = strtol(argv[1], NULL, 10);
  int fd = CreateServer("127.0.0.1", port);
  pthread_t worker;
  int create_result = pthread_create(&worker, NULL, ServerThread, &fd);
  if (create_result != 0) {
    FatalError("failed to run server thread: %d\n", create_result);
  }
  int join_result = pthread_join(worker, NULL);
  if (join_result != 0) {
    FatalError("failed to join server thread: %d\n", join_result);
  }
  printf("gracefully exiting");

  return 0;
}