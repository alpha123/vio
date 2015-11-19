#ifndef VIO_SERVER_H
#define VIO_SERVER_H

#include "wby.h"

struct vio_server_state {
    vio_ctx *ctx;
    int enable_webrepl;
    struct wby_con *replclient;
};

#ifdef VIO_WEBREPL
void vio_server_start(int port, int repl);
#else
void vio_server_start(int port);
#endif

#endif
