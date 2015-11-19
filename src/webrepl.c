#include <stdio.h>
#include "context.h"
#include "eval.h"
#include "uneval.h"
#include "server.h"
#include "serialize.h"
#include "webrepl.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define MAX_FRAME_PAYLOAD (2*1024)

int vio_webrepl_connect(struct wby_con *conn, void *st) {
    struct vio_server_state *s = (struct vio_server_state *)st;
    conn->user_data = NULL;
    /* 0 - connect, 1 - refuse */
    if (s->enable_webrepl && strcmp(conn->request.uri, "/webrepl/socket") == 0)
        return 0;
    return 1;
}

void vio_webrepl_connected(struct wby_con *conn, void *st) {
    /* at the moment, we only support 1 websocket connection */
    struct vio_server_state *s = (struct vio_server_state *)st;
    s->replclient = conn;
}

int vio_webrepl_frame(struct wby_con *conn, const struct wby_frame *frame, void *st) {
    struct vio_server_state *s = (struct vio_server_state *)st;
    char str[MAX_FRAME_PAYLOAD];
    vio_err_t err = 0;
    if (frame->payload_length > MAX_FRAME_PAYLOAD)
        return 1;
    if (wby_read(conn, str, frame->payload_length) != 0)
        return 1;

    wby_frame_begin(s->replclient, WBY_WSOP_TEXT_FRAME);
    if ((err = vio_eval(s->ctx, frame->payload_length, str))) {
        wby_write(s->replclient, "ERROR", 5);
        wby_write(s->replclient, vio_err_msg(err), strlen(vio_err_msg(err)));
        wby_write(s->replclient, "\n", 1);
        wby_write(s->replclient, s->ctx->err_msg, strlen(s->ctx->err_msg));
    }
    else {
        const char *jsrepr = vio_json_stack(s->ctx);
        wby_write(s->replclient, jsrepr, strlen(jsrepr));
        free((char *)jsrepr);
    }
    wby_frame_end(s->replclient);
    return 0;
}

void vio_webrepl_closed(struct wby_con *conn, void *st) {
    struct vio_server_state *s = (struct vio_server_state *)st;
    FILE *img = fopen("webrepl.vio", "wb");
    vio_dump(s->ctx, img);
    fclose(img);
}
