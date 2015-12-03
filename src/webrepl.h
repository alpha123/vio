#ifndef VIO_WEBREPL_H
#define VIO_WEBREPL_H

#include "server.h"

int vio_webrepl_serve(struct mg_connection *conn, void *_cbdata);
int vio_webrepl_serve_js(struct mg_connection *conn, void *_cbdata);
int vio_webrepl_serve_wiki(struct mg_connection *conn, void *_cbdata);
int vio_webrepl_save_wiki(struct mg_connection *conn, void *_cbdata);
int vio_webrepl_wsstart(struct mg_connection *conn, void *_cbdata);
int vio_webrepl_wsconnect(const struct mg_connection *conn, void *_cbdata);
void vio_webrepl_wsready(struct mg_connection *conn, void *_cbdata);
int vio_webrepl_wsdata(struct mg_connection *conn, int bits, char *data,
                       size_t len, void *_cbdata);
void vio_webrepl_wsclose(const struct mg_connection *conn, void *_cbdata);

#endif
