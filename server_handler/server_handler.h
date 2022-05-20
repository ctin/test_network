#pragma once

#include "async_server/async_server.h"

void OnData(ServerEvent *e);
void OnAccept(ServerEvent *e);
void OnError(ServerEvent *e);