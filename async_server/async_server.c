#include "async_server.h"
#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#define _DARWIN_UNLIMITED_SELECT
#endif
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>

#include "async_server/async_server.h"
#include "utils/error_handler.h"
#include "utils/realloc.h"
#include "async_server/vec.h"

#ifndef INVALID_SOCKET
  #define INVALID_SOCKET -1
#endif


/*===========================================================================*/
/* Memory                                                                    */
/*===========================================================================*/

static double ServergetTime(void);
static void Serverclose(ServerStream *stream);

enum {
  SELECT_READ,
  SELECT_WRITE,
  SELECT_EXCEPT,
  SELECT_MAX
};

typedef struct {
  int capacity;
  int maxfd;
  fd_set *fds[SELECT_MAX];
} SelectSet;

#define SERVER_UNSIGNED_BIT (sizeof(unsigned) * CHAR_BIT)


static void select_deinit(SelectSet *s) {
  int i;
  for (i = 0; i < SELECT_MAX; i++) {
    free(s->fds[i]);
    s->fds[i] = NULL;
  }
  s->capacity = 0;
}


static void select_grow(SelectSet *s) {
  int i;
  int oldCapacity = s->capacity;
  s->capacity = s->capacity ? s->capacity << 1 : 1;
  for (i = 0; i < SELECT_MAX; i++) {
    s->fds[i] = Realloc(s->fds[i], s->capacity * sizeof(fd_set));
    memset(s->fds[i] + oldCapacity, 0,
           (s->capacity - oldCapacity) * sizeof(fd_set));
  }
}


static void select_zero(SelectSet *s) {
  int i;
  if (s->capacity == 0) return;
  s->maxfd = 0;
  for (i = 0; i < SELECT_MAX; i++) {
    memset(s->fds[i], 0, s->capacity * sizeof(fd_set));
  }
}


static void select_add(SelectSet *s, int set, int fd) {
  unsigned *p;
  while (s->capacity * FD_SETSIZE < fd) {
    select_grow(s);
  }
  p = (unsigned*) s->fds[set];
  p[fd / SERVER_UNSIGNED_BIT] |= 1 << (fd % SERVER_UNSIGNED_BIT);
  if (fd > s->maxfd) s->maxfd = fd;
}


static int select_has(SelectSet *s, int set, int fd) {
  unsigned *p;
  if (s->maxfd < fd) return 0;
  p = (unsigned*) s->fds[set];
  return p[fd / SERVER_UNSIGNED_BIT] & (1 << (fd % SERVER_UNSIGNED_BIT));
}


/*===========================================================================*/
/* Core                                                                      */
/*===========================================================================*/

typedef struct {
  int event;
  ServerCallback callback;
} Listener;


struct ServerStream {
  int state, flags;
  int sockfd;
  char *address;
  int port;
  int bytesSent, bytesReceived;
  double lastActivity, timeout;
  Vec(Listener) listeners;
  Vec(char) lineBuffer;
  Vec(char) writeBuffer;
  ServerStream *next;
};

#define ServerFLAG_READY   (1 << 0)
#define ServerFLAG_WRITTEN (1 << 1)


static ServerStream *server_streams;
static int server_streams_count;
static SelectSet server_select_set;
static double server_update_timeout = 1;
static double server_tick_interval = 1;
static double server_last_tick = 0;


static ServerEvent ServerCreateEvent(int type) {
  ServerEvent e;
  memset(&e, 0, sizeof(e));
  e.type = type;
  return e;
}

static void ServerDestroyStream(ServerStream *stream);

static void ServerDestroyClosedStreams(void) {
  ServerStream *stream = server_streams;
  while (stream) {
    if (stream->state == SERVER_STATE_CLOSED) {
      ServerStream *next = stream->next;
      ServerDestroyStream(stream);
      stream = next;
    } else {
      stream = stream->next;
    }
  }
}

static void ServerEmitEvent(ServerStream *stream, ServerEvent *e);

static void updateTickTimer(void) {
  /* Update tick timer */
  if (server_last_tick == 0) {
    server_last_tick = ServergetTime();
  }
  while (server_last_tick < ServergetTime()) {
    /* Emit event on all streams */
    ServerStream *stream;
    ServerEvent e = ServerCreateEvent(SERVER_EVENT_TICK);
    e.msg = "a tick has occured";
    stream = server_streams;
    while (stream) {
      ServerEmitEvent(stream, &e);
      stream = stream->next;
    }
    server_last_tick += server_tick_interval;
  }
}

void ServerCloseAllStreams(void) {
  ServerStream *stream;
  stream = server_streams;
  while (stream) {
    Serverclose(stream);
    stream = stream->next;
  }
}

static void ServerUpdateStreamTimeouts(void) {
  double currentTime = ServergetTime();
  ServerStream *stream;
  ServerEvent e = ServerCreateEvent(SERVER_EVENT_TIMEOUT);
  e.msg = "stream timed out";
  stream = server_streams;
  while (stream) {
    if (stream->timeout) {
      if (currentTime - stream->lastActivity > stream->timeout) {
        ServerEmitEvent(stream, &e);
        Serverclose(stream);
      }
    }
    stream = stream->next;
  }
}



/*===========================================================================*/
/* Stream                                                                    */
/*===========================================================================*/

static void ServerDestroyStream(ServerStream *stream) {
  ServerEvent e;
  ServerStream **next;
  /* Close socket */
  if (stream->sockfd != INVALID_SOCKET) {
    close(stream->sockfd);
  }
  /* Emit destroy event */
  e = ServerCreateEvent(SERVER_EVENT_DESTROY);
  e.msg = "the stream has been destroyed";
  ServerEmitEvent(stream, &e);
  /* Remove from list and decrement count */
  next = &server_streams;
  while (*next != stream) {
    next = &(*next)->next;
  }
  *next = stream->next;
  server_streams_count--;
  /* Destroy and free */
  VecDeinit(&stream->listeners);
  VecDeinit(&stream->lineBuffer);
  VecDeinit(&stream->writeBuffer);
  free(stream->address);
  free(stream);
}


static void ServerEmitEvent(ServerStream *stream, ServerEvent *e) {
  int i;
  e->stream = stream;
  for (i = 0; i < stream->listeners.length; i++) {
    Listener *listener = &stream->listeners.data[i];
    if (listener->event == e->type) {
      listener->callback(e);
    }
    /* Check to see if this listener was removed: If it was we decrement `i`
     * since the next listener will now be in this ones place */
    if (listener != &stream->listeners.data[i]) {
      i--;
    }
  }
}


static void ServerError(ServerStream *stream, const char *msg, int err) {
  char buf[256];
  ServerEvent e = ServerCreateEvent(SERVER_EVENT_ERROR);
  if (err) {
    sprintf(buf, "%.160s (%.80s)", msg, strerror(err));
    e.msg = buf;
  } else {
    e.msg = msg;
  }
  ServerEmitEvent(stream, &e);
  Serverclose(stream);
}


static void ServerInitAddress(ServerStream *stream) {
  union { struct sockaddr sa; struct sockaddr_storage sas;
          struct sockaddr_in sai; struct sockaddr_in6 sai6; } addr;
  socklen_t size;
  memset(&addr, 0, sizeof(addr));
  size = sizeof(addr);
  free(stream->address);
  stream->address = NULL;
  if (getpeername(stream->sockfd, &addr.sa, &size) == -1) {
    if (getsockname(stream->sockfd, &addr.sa, &size) == -1) {
      return;
    }
  }
  if (addr.sas.ss_family == AF_INET6) {
    stream->address = Realloc(NULL, INET6_ADDRSTRLEN);
    inet_ntop(AF_INET6, &addr.sai6.sin6_addr, stream->address,
              INET6_ADDRSTRLEN);
    stream->port = ntohs(addr.sai6.sin6_port);
  } else {
    stream->address = Realloc(NULL, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &addr.sai.sin_addr, stream->address, INET_ADDRSTRLEN);
    stream->port = ntohs(addr.sai.sin_port);
  }
}


static void ServerSetSocketNonBlocking(ServerStream *stream, int opt) {

  int flags = fcntl(stream->sockfd, F_GETFL);
  fcntl(stream->sockfd, F_SETFL,
        opt ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
}


static void ServerSetSocket(ServerStream *stream, int sockfd) {
  stream->sockfd = sockfd;
  ServerSetSocketNonBlocking(stream, 1);
  ServerInitAddress(stream);
}


static int ServerInitSocket(
  ServerStream *stream, int domain, int type, int protocol
) {
  stream->sockfd = socket(domain, type, protocol);
  if (stream->sockfd == INVALID_SOCKET) {
    ServerError(stream, "could not create socket", errno);
    return -1;
  }
  ServerSetSocket(stream, stream->sockfd);
  return 0;
}


static int ServerHasListenerForEvent(ServerStream *stream, int event) {
  int i;
  for (i = 0; i < stream->listeners.length; i++) {
    Listener *listener = &stream->listeners.data[i];
    if (listener->event == event) {
      return 1;
    }
  }
  return 0;
}


static void ServerHandleReceivedData(ServerStream *stream) {
  for (;;) {
    /* Receive data */
    ServerEvent e;
    char data[PACKET_SIZE];
    int size = recv(stream->sockfd, data, sizeof(data) - 1, 0);
    if (size <= 0) {
      if (size == 0 || errno != EWOULDBLOCK) {
        /* Handle disconnect */
        Serverclose(stream);
        return;
      } else {
        /* No more data */
        return;
      }
    }
    data[size] = 0;
    /* Update status */
    stream->bytesReceived += size;
    stream->lastActivity = ServergetTime();
    /* Emit data event */
    e = ServerCreateEvent(SERVER_EVENT_DATA);
    e.msg = "received data";
    e.data = data;
    e.size = size;
    ServerEmitEvent(stream, &e);
    /* Check stream state in case it was closed during one of the data event
     * handlers. */
    if (stream->state != SERVER_STATE_CONNECTED) {
      return;
    }

    /* Handle line event */
    if (ServerHasListenerForEvent(stream, SERVER_EVENT_LINE)) {
      int i, start;
      char *buf;
      for (i = 0; i < size; i++) {
        VecPush(&stream->lineBuffer, data[i]);
      }
      start = 0;
      buf = stream->lineBuffer.data;
      for (i = 0; i < stream->lineBuffer.length; i++) {
        if (buf[i] == '\n') {
          ServerEvent e;
          buf[i] = '\0';
          e = ServerCreateEvent(SERVER_EVENT_LINE);
          e.msg = "received line";
          e.data = &buf[start];
          e.size = i - start;
          /* Check and strip carriage return */
          if (e.size > 0 && e.data[e.size - 1] == '\r') {
            e.data[--e.size] = '\0';
          }
          ServerEmitEvent(stream, &e);
          start = i + 1;
          /* Check stream state in case it was closed during one of the line
           * event handlers. */
          if (stream->state != SERVER_STATE_CONNECTED) {
            return;
          }
        }
      }
      if (start == stream->lineBuffer.length) {
        VecClear(&stream->lineBuffer);
      } else {
        VecSplice(&stream->lineBuffer, 0, start);
      }
    }
  }
}


static void ServerAcceptPendingConnections(ServerStream *stream) {
  for (;;) {
    ServerStream *remote;
    ServerEvent e;
    int err = 0;
    int sockfd = accept(stream->sockfd, NULL, NULL);
    if (sockfd == INVALID_SOCKET) {
      err = errno;
      if (err == EWOULDBLOCK) {
        /* No more waiting sockets */
        return;
      }
    }
    /* Create client stream */
    remote = ServerNewStream();
    remote->state = SERVER_STATE_CONNECTED;
    /* Set stream's socket */
    ServerSetSocket(remote, sockfd);
    /* Emit accept event */
    e = ServerCreateEvent(SERVER_EVENT_ACCEPT);
    e.msg = "accepted connection";
    e.remote = remote;
    ServerEmitEvent(stream, &e);
    /* Handle invalid socket -- the stream is still made and the ACCEPT event
     * is still emitted, but its shut immediately with an error */
    if (remote->sockfd == INVALID_SOCKET) {
      ServerError(remote, "failed to create socket on accept", err);
      return;
    }
  }
}


static int ServerFlushWriteBuffer(ServerStream *stream) {
  stream->flags &= ~ServerFLAG_WRITTEN;
  if (stream->writeBuffer.length > 0) {
    /* Send data */
    int size = send(stream->sockfd, stream->writeBuffer.data,
                    stream->writeBuffer.length, 0);
    if (size <= 0) {
      if (errno == EWOULDBLOCK) {
        /* No more data can be written */
        return 0;
      } else {
        /* Handle disconnect */
        Serverclose(stream);
        return 0;
      }
    }
    if (size == stream->writeBuffer.length) {
      VecClear(&stream->writeBuffer);
    } else {
      VecSplice(&stream->writeBuffer, 0, size);
    }
    /* Update status */
    stream->bytesSent += size;
    stream->lastActivity = ServergetTime();
  }

  if (stream->writeBuffer.length == 0) {
    ServerEvent e;
    /* If this is a 'closing' stream we can properly close it now */
    if (stream->state == SERVER_STATE_CLOSING) {
      Serverclose(stream);
      return 0;
    }
    /* Set ready flag and emit 'ready for data' event */
    stream->flags |= ServerFLAG_READY;
    e = ServerCreateEvent(SERVER_EVENT_READY);
    e.msg = "stream is ready for more data";
    ServerEmitEvent(stream, &e);
  }
  /* Return 1 to indicate that more data can immediately be written to the
   * stream's socket */
  return 1;
}



/*===========================================================================*/
/* API                                                                       */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/* Core                                                                      */
/*---------------------------------------------------------------------------*/

void ServerUpdate(void) {
  ServerStream *stream;
  struct timeval tv;

  ServerDestroyClosedStreams();
  updateTickTimer();
  ServerUpdateStreamTimeouts();

  /* Create fd sets for select() */
  select_zero(&server_select_set);

  stream = server_streams;
  while (stream) {
    switch (stream->state) {
      case SERVER_STATE_CONNECTED:
        select_add(&server_select_set, SELECT_READ, stream->sockfd);
        if (!(stream->flags & ServerFLAG_READY) ||
            stream->writeBuffer.length != 0
        ) {
          select_add(&server_select_set, SELECT_WRITE, stream->sockfd);
        }
        break;
      case SERVER_STATE_CLOSING:
        select_add(&server_select_set, SELECT_WRITE, stream->sockfd);
        break;
      case SERVER_STATE_CONNECTING:
        select_add(&server_select_set, SELECT_WRITE, stream->sockfd);
        select_add(&server_select_set, SELECT_EXCEPT, stream->sockfd);
        break;
      case SERVER_STATE_LISTENING:
        select_add(&server_select_set, SELECT_READ, stream->sockfd);
        break;
    }
    stream = stream->next;
  }


  tv.tv_sec = server_update_timeout;
  tv.tv_usec = (server_update_timeout - tv.tv_sec) * 1e6;

  select(server_select_set.maxfd + 1,
         server_select_set.fds[SELECT_READ],
         server_select_set.fds[SELECT_WRITE],
         server_select_set.fds[SELECT_EXCEPT],
         &tv);

  /* Handle streams */
  stream = server_streams;
  while (stream) {
    switch (stream->state) {

      case SERVER_STATE_CONNECTED:
        if (select_has(&server_select_set, SELECT_READ, stream->sockfd)) {
          ServerHandleReceivedData(stream);
          if (stream->state == SERVER_STATE_CLOSED) {
            break;
          }
        }
        /* Fall through */

      case SERVER_STATE_CLOSING:
        if (select_has(&server_select_set, SELECT_WRITE, stream->sockfd)) {
          ServerFlushWriteBuffer(stream);
        }
        break;

      case SERVER_STATE_CONNECTING:
        if (select_has(&server_select_set, SELECT_WRITE, stream->sockfd)) {
          /* Check socket for error */
          int optval = 0;
          socklen_t optlen = sizeof(optval);
          ServerEvent e;
          getsockopt(stream->sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen);
          if (optval != 0) goto connectFailed;
          /* Handle succeselful connection */
          stream->state = SERVER_STATE_CONNECTED;
          stream->lastActivity = ServergetTime();
          ServerInitAddress(stream);
          /* Emit connect event */
          e = ServerCreateEvent(SERVER_EVENT_CONNECT);
          e.msg = "connected to server";
          ServerEmitEvent(stream, &e);
        } else if (
          select_has(&server_select_set, SELECT_EXCEPT, stream->sockfd)
        ) {
          /* Handle failed connection */
connectFailed:
          ServerError(stream, "could not connect to server", 0);
        }
        break;

      case SERVER_STATE_LISTENING:
        if (select_has(&server_select_set, SELECT_READ, stream->sockfd)) {
          ServerAcceptPendingConnections(stream);
        }
        break;
    }

    /* If data was just now written to the stream we should immediately try to
     * send it */
    if (
      stream->flags & ServerFLAG_WRITTEN &&
      stream->state != SERVER_STATE_CLOSED
    ) {
      ServerFlushWriteBuffer(stream);
    }

    stream = stream->next;
  }
}


void ServerInit(void) {
  /* Stops the SIGPIPE signal being raised when writing to a closed socket */
  signal(SIGPIPE, SIG_IGN);
}

double ServergetTime(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec / 1e6;
}


int ServerGetStreamsCount(void) {
  return server_streams_count;
}

/*---------------------------------------------------------------------------*/
/* Stream                                                                    */
/*---------------------------------------------------------------------------*/

ServerStream *ServerNewStream(void) {
  ServerStream *stream = Realloc(NULL, sizeof(*stream));
  memset(stream, 0, sizeof(*stream));
  stream->state = SERVER_STATE_CLOSED;
  stream->sockfd = INVALID_SOCKET;
  stream->lastActivity = ServergetTime();
  /* Add to list and increment count */
  stream->next = server_streams;
  server_streams = stream;
  server_streams_count++;
  return stream;
}


void ServerAddListener(
  ServerStream *stream, int event, ServerCallback callback) {
  Listener listener;
  listener.event = event;
  listener.callback = callback;
  VecPush(&stream->listeners, listener);
}


void Serverclose(ServerStream *stream) {
  ServerEvent e;
  if (stream->state == SERVER_STATE_CLOSED) return;
  stream->state = SERVER_STATE_CLOSED;
  /* Close socket */
  if (stream->sockfd != INVALID_SOCKET) {
    close(stream->sockfd);
    stream->sockfd = INVALID_SOCKET;
  }
  /* Emit event */
  e = ServerCreateEvent(SERVER_EVENT_CLOSE);
  e.msg = "stream closed";
  ServerEmitEvent(stream, &e);
  /* Clear buffers */
  VecClear(&stream->lineBuffer);
  VecClear(&stream->writeBuffer);
}

int ServerListenEx(
  ServerStream *stream, const char *host, int port, int backlog
) {
  struct addrinfo hints, *ai = NULL;
  int err, optval;
  char buf[64];
  ServerEvent e;

  /* Get addrinfo */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  sprintf(buf, "%d", port);
  err = getaddrinfo(host, buf, &hints, &ai);
  if (err) {
    ServerError(stream, "could not get addrinfo", errno);
    goto fail;
  }
  /* Init socket */
  err = ServerInitSocket(stream, ai->ai_family, ai->ai_socktype,
                          ai->ai_protocol);
  if (err) goto fail;
  /* Set SO_REUSEADDR so that the socket can be immediately bound without
   * having to wait for any closed socket on the same port to timeout */
  optval = 1;
  setsockopt(stream->sockfd, SOL_SOCKET, SO_REUSEADDR,
             &optval, sizeof(optval));
  /* Bind and listen */
  err = bind(stream->sockfd, ai->ai_addr, ai->ai_addrlen);
  if (err) {
    ServerError(stream, "could not bind socket", errno);
    goto fail;
  }
  err = listen(stream->sockfd, backlog);
  if (err) {
    ServerError(stream, "socket failed on listen", errno);
    goto fail;
  }
  stream->state = SERVER_STATE_LISTENING;
  stream->port = port;
  ServerInitAddress(stream);
  /* Emit listening event */
  e = ServerCreateEvent(SERVER_EVENT_LISTEN);
  e.msg = "socket is listening";
  ServerEmitEvent(stream, &e);
  freeaddrinfo(ai);
  return 0;
  fail:
  if (ai) freeaddrinfo(ai);
  return -1;
}


int ServerListen(ServerStream *stream, int port) {
  return ServerListenEx(stream, NULL, port, 511);
}


#ifndef TEMP_BUFFER_SIZE
#define TEMP_BUFFER_SIZE 1024
#endif //TEMP_BUFFER_SIZE

void ServerWritef(ServerStream *stream, const char* message, ...) {
    char output[TEMP_BUFFER_SIZE] = {0};
    va_list argptr;
    va_start(argptr, message);
    vsnprintf (output, TEMP_BUFFER_SIZE - 1, message, argptr);
    va_end(argptr);
    int size = TEMP_BUFFER_SIZE;
    char *p = &output[0];
    while (size--) {
        VecPush(&stream->writeBuffer, *p++);
  }
}


void ServerWrite(ServerStream *stream, const void *data, int size) {
  const char *p = data;
  while (size--) {
    VecPush(&stream->writeBuffer, *p++);
  }
  stream->flags |= ServerFLAG_WRITTEN;
}
