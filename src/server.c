#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include "context.h"
#include "serialize.h"
#include "webrepl.h"
#include "server.h"

static struct mg_context *server;

void vio_server_stop_(int _) {
    mg_stop(server);
    puts("\nBye!");
    exit(0);
}

void vio_server_stop(void) {
    vio_server_stop_(0);
}

void vio_server_start(int port) {
    struct vio_server_state st;
    struct mg_callbacks cb;
    struct mg_context *serv;
    const char *options[] = {
        "document_root", ".",
        "listening_ports", "",
        "request_timeout_ms", "10000",
        "websocket_timeout_ms", "3600000",
        0
    };
    char portstr[6];
    options[3] = portstr;
    snprintf(portstr, 6, "%d", port);

    memset(&cb, 0, sizeof(struct mg_callbacks));
    memset(st.replclients, 0, sizeof(struct mg_connection *) * VIO_MAX_WS_CLIENTS);

    serv = mg_start(&cb, &st, options);

    mg_set_request_handler(serv, "**.vio$", vio_webrepl_serve, 0);
    mg_set_request_handler(serv, "^/webrepl/bundle.js$", vio_webrepl_serve_js, 0);
    mg_set_request_handler(serv, "**.wiki.html$", vio_webrepl_serve_wiki, 0);
    mg_set_request_handler(serv, "**.wiki.html/upload$", vio_webrepl_save_wiki, 0);
    mg_set_request_handler(serv, "/webrepl/socket", vio_webrepl_wsstart, 0);
    mg_set_websocket_handler(serv, "/webrepl/socket",
                             vio_webrepl_wsconnect,
                             vio_webrepl_wsready,
                             vio_webrepl_wsdata,
                             vio_webrepl_wsclose,
                             0);
    server = serv;

    struct sigaction sa;
    sa.sa_handler = vio_server_stop_;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGINT);
    sigsuspend(&mask);
}
