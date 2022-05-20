#pragma once

#include <stdarg.h>

struct ServerStream;
typedef struct ServerStream ServerStream;

#define PACKET_SIZE 8192

typedef struct {
  int type;
  ServerStream *stream;
  ServerStream *remote;
  const char *msg;
  char *data;
  int size;
} ServerEvent;

typedef void (*ServerCallback)(ServerEvent*);

enum {
  SERVER_EVENT_NULL,
  SERVER_EVENT_DESTROY,
  SERVER_EVENT_ACCEPT,
  SERVER_EVENT_LISTEN,
  SERVER_EVENT_CONNECT,
  SERVER_EVENT_CLOSE,
  SERVER_EVENT_READY,
  SERVER_EVENT_DATA,
  SERVER_EVENT_LINE,
  SERVER_EVENT_ERROR,
  SERVER_EVENT_TIMEOUT,
  SERVER_EVENT_TICK
};

enum {
  SERVER_STATE_CLOSED,
  SERVER_STATE_CLOSING,
  SERVER_STATE_CONNECTING,
  SERVER_STATE_CONNECTED,
  SERVER_STATE_LISTENING
};


void ServerInit(void);
void ServerUpdate(void);
int  ServerGetStreamsCount(void);

ServerStream *ServerNewStream(void);
int  ServerListen(ServerStream *stream, int port);
void ServerAddListener(ServerStream *stream, int event,
                      ServerCallback callback);
void ServerWritef(ServerStream *stream, const char* message, ...);
void ServerWrite(ServerStream *stream, const void *data, int size);
void ServerCloseAllStreams(void);
