#include <unistd.h>
#include <magic.h>

#include "context.h"
#include "serialize.h"
#ifdef VIO_WEBREPL
#include "webrepl.h"
#endif
#include "server.h"

wby_int vio_serve(struct wby_con *conn, void *st) {
    FILE *f;
    uint32_t ulen = strlen(conn->request.uri);
    if (ulen == 1) /* uri == "/" */
        f = fopen("index.html", "r");
    else {
        /* nice security */
        f = fopen(conn->request.uri + 1, "r"); /* remove the leading / */
    }
    char buf[4096];
    struct wby_header contenttype;
    contenttype.name = "Content-type";
    uint32_t rsz;
    if (f) {
        /* libmagic doesn't detect mimetypes for text/css and application/javascript properly */
        if (ulen > 4 && strncmp(conn->request.uri + (ulen - 4), ".css", 4) == 0)
            contenttype.value = "text/css";
        else if (ulen > 3 && strncmp(conn->request.uri + (ulen - 3), ".js", 3) == 0)
            contenttype.value = "application/javascript";
        else {
            magic_t mime = magic_open(MAGIC_MIME_TYPE);
            magic_load(mime, NULL);
            contenttype.value = magic_file(mime, conn->request.uri + 1);
            magic_close(mime);
        }
        
        wby_response_begin(conn, 200, -1, &contenttype, 1);
        while (!feof(f)) {
            rsz = fread(buf, 1, sizeof(buf), f);
            wby_write(conn, buf, rsz);
        }
        wby_response_end(conn);
        fclose(f);
        return 0;
    }
    return 1;
}

static struct wby_server *me;

void vio_server_log(const char *msg) {
#ifndef NDEBUG
    printf("[vio_serve] %s\n", msg);
#endif
}

void vio_server_stop(void) {
    wby_server_stop(me);
}

#ifdef VIO_WEBREPL
void vio_server_start(int port, int repl) {
#else
void vio_server_start(int port) {
#endif
    vio_ctx ctx;
    struct vio_server_state st;
    struct wby_config cfg;
    cfg.address = "127.0.0.1";
    cfg.port = port;
    cfg.connection_max = 2;
    cfg.request_buffer_size = 8 * 1024;
    cfg.io_buffer_size = 16 * 1024;
    cfg.log = vio_server_log;
    cfg.userdata = &st;
    cfg.dispatch = vio_serve;
#ifdef VIO_WEBREPL
    if (repl) {
        st.enable_webrepl = 1;
        vio_open(&ctx);
        if (access("webrepl.vio", R_OK) != -1) {
            FILE *img = fopen("webrepl.vio", "rb");
            vio_load(&ctx, img);
            fclose(img);
        }
        st.ctx = &ctx;
        st.replclient = NULL; /* not connected yet */
        cfg.ws_connect = vio_webrepl_connect;
        cfg.ws_connected = vio_webrepl_connected;
        cfg.ws_frame = vio_webrepl_frame;
        cfg.ws_closed = vio_webrepl_closed;
    }
#else
    if (0);
#endif
    else {
        st.enable_webrepl = 0;
        st.ctx = NULL;
        st.replclient = NULL;
    }

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
