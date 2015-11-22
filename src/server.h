#ifndef VIO_SERVER_H
#define VIO_SERVER_H

#include "civetweb.h"

#define VIO_MAX_WS_CLIENTS 5

struct vio_server_state {
    vio_ctx *ctx;
    struct mg_connection *replclients[VIO_MAX_WS_CLIENTS];
};

void vio_server_start(int port);
void vio_server_stop(void);

#endif
