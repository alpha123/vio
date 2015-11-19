#ifndef VIO_WEBREPL_H
#define VIO_WEBREPL_H

#include "server.h"

int vio_webrepl_connect(struct wby_con *conn, void *st);
void vio_webrepl_connected(struct wby_con *conn, void *st);
int vio_webrepl_frame(struct wby_con *conn, const struct wby_frame *frame, void *st);
void vio_webrepl_closed(struct wby_con *conn, void *st);

#endif
