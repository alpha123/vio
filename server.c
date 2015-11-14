#include <magic.h>

#define WBY_IMPLEMENTATION
#include "wby.h"
#include "server.h"

wby_int vio_serve(struct wby_con *conn, void *arg) {
    /* nice security */
    FILE *f = fopen(conn->request.uri + 1, "r"); /* remove the leading / */
    char buf[4096];
    struct wby_header contenttype;
    contenttype.name = "Content-type";
    uint32_t rsz;
    if (f) {
        magic_t mime = magic_open(MAGIC_MIME_TYPE);
        magic_load(mime, NULL);
        contenttype.value = magic_file(mime, conn->request.uri + 1);
        magic_close(mime);
        
        wby_response_begin(conn, 200, -1, &contenttype, 1);
        while (!feof(f)) {
            rsz = fread(buf, 1, sizeof(buf), f);
            wby_write(conn, buf, rsz);
        }
        wby_response_end(conn);
        fclose(f);
        return 0;
    }
    else {
        wby_response_begin(conn, 404, 10, NULL, 0);
        wby_write(conn, "Not found\n", 10);
        wby_response_end(conn);
        return 1;
    }
}

static struct wby_server *me;

void vio_server_log(const char *msg) {
    printf("[vio_serve] %s\n", msg);
}

void vio_server_stop(void) {
    wby_server_stop(me);
}

void vio_server_start(int port) {
    struct wby_config cfg;
    cfg.address = "127.0.0.1";
    cfg.port = port;
    cfg.connection_max = 2;
    cfg.request_buffer_size = 2048;
    cfg.io_buffer_size = 8192;
    cfg.log = vio_server_log;
    cfg.dispatch = vio_serve;

    struct wby_server serv;
    wby_size mem;
    wby_server_init(&serv, &cfg, &mem);
    void *block = calloc(mem, 1);
    wby_server_start(&serv, block);
    me = &serv;
    atexit(vio_server_stop);

    while (1)
        wby_server_update(&serv);
}
