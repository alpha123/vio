#include <stdio.h>
#include "context.h"
#include "eval.h"
#include "uneval.h"
#include "server.h"
#include "serialize.h"
#include "stdvio.h"
#include "lzf.h"
#include "webrepl.h"

#include "webrepl_html.i"
#include "webrepl_js.i"
#include "webrepl_wiki.i"

struct ctx_info {
    char *image_file;
    vio_ctx *ctx;
};

int vio_webrepl_serve(struct mg_connection *conn, void *_cbdata) {
    struct ctx_info *ci = (struct ctx_info *)malloc(sizeof(struct ctx_info));
    /* strip leading / */
    const char *image_file = mg_get_request_info(conn)->local_uri + 1;

    ci->image_file = (char *)malloc(strlen(image_file));
    strcpy(ci->image_file, image_file);

    ci->ctx = (vio_ctx *)malloc(sizeof(vio_ctx));

    if (vio_open_image(ci->ctx, image_file) == VE_IO_FAIL)
        vio_open(ci->ctx);
    vio_load_stdlib(ci->ctx);

    mg_set_user_connection_data(conn, ci);

    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n");
    mg_write(conn, webrepl_index_html, webrepl_index_html_len);

    return 1;
}

int vio_webrepl_serve_js(struct mg_connection *conn, void *_cbdata) {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-type: application/javascriptr\n\r\n");
    mg_write(conn, webrepl_bundle_js, webrepl_bundle_js_len);
    return 1;
}

int vio_webrepl_serve_wiki(struct mg_connection *conn, void *_cbdata) {
    uint8_t *data = malloc(webrepl_wiki_html_len);
    uint32_t out_len = lzf_decompress(
        webrepl_wiki_html_lzf, webrepl_wiki_html_lzf_len,
        data, webrepl_wiki_html_len);

    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n");
    mg_write(conn, data, out_len);
    free(data);

    return 1;
}

int vio_webrepl_wsstart(struct mg_connection *conn, void *_cbdata) {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-type: text/plain\r\n\r\n");
    return 1;
}

int vio_webrepl_wsconnect(const struct mg_connection *conn, void *_cbdata) {
    return 0;
}

void vio_webrepl_wsready(struct mg_connection *conn, void *_cbdata) {
}

int vio_webrepl_wsdata(struct mg_connection *conn, int bits, char *data,
                       size_t len, void *_cbdata) {
    struct ctx_info *ci = mg_get_user_connection_data(conn);
    vio_err_t err = vio_eval(ci->ctx, len, data);
    char err_msg[255];
    if (err) {
        snprintf(err_msg, 255, "ERROR\n%.80s\n%.170s", vio_err_msg(err), ci->ctx->err_msg);
        mg_websocket_write(conn, WEBSOCKET_OPCODE_TEXT, err_msg, strlen(err_msg));
    }
    else {
        const char *s = vio_json_stack(ci->ctx);
        mg_websocket_write(conn, WEBSOCKET_OPCODE_TEXT, s, strlen(s));
        free((char *)s); /* this however is not */
    }
    return 1;
}

void vio_webrepl_wsclose(const struct mg_connection *conn, void *_cbdata) {
    struct ctx_info *ci = mg_get_user_connection_data(conn);
    vio_close_image(ci->ctx, ci->image_file);
    free(ci->image_file);
    free(ci->ctx);
    free(ci);
}
