#include <stdio.h>
#include <sys/stat.h>
#include "context.h"
#include "eval.h"
#include "uneval.h"
#include "server.h"
#include "serialize.h"
#include "stdvio.h"
#include "fse.h"
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

    mg_printf(conn,
              "HTTP/1.1 200 OK\r\n"
              "Content-length: %d\r\n"
              "Content-type: text/html\r\n\r\n",
              webrepl_index_html_len);
    mg_write(conn, webrepl_index_html, webrepl_index_html_len);

    return 1;
}

int vio_webrepl_serve_js(struct mg_connection *conn, void *_cbdata) {
    mg_printf(conn,
              "HTTP/1.1 200 OK\r\n"
              "Content-length: %d\r\n"
              "Content-type: application/javascriptr\n\r\n",
              webrepl_bundle_js_len);
    mg_write(conn, webrepl_bundle_js, webrepl_bundle_js_len);
    return 1;
}

int vio_webrepl_serve_wiki(struct mg_connection *conn, void *_cbdata) {
    FILE *f;
    if ((f = fopen(mg_get_request_info(conn)->local_uri + 1, "r")))
        mg_send_file(conn, mg_get_request_info(conn)->local_uri + 1);
    else {
        uint32_t in_len = webrepl_wiki_html_lzf_len, out_len = webrepl_wiki_html_len;
        uint8_t *out = malloc(out_len), *in = webrepl_wiki_html_lzf;
        /*in_len = FSE_decompress(out, out_len, in, in_len);
        if (FSE_isError(in_len))
            printf("\n\nUH OH!\n%s\n\n", FSE_getErrorName(in_len));
        in = out;*/
        out_len = lzf_decompress(in, in_len, out, out_len);

        mg_printf(conn,
                  "HTTP/1.1 200 OK\r\n"
                  "Content-length: %d\r\n"
                  "Content-type: text/html\r\n\r\n",
                  out_len);
        mg_write(conn, out, out_len);
        free(out);
    }

    return 1;
}

int vio_webrepl_save_wiki(struct mg_connection *conn, void *_cbdata) {
    if (strcmp(mg_get_request_info(conn)->request_method, "POST") == 0) {
        uint8_t buf[2048];
        uint32_t total = 0;
        int r, off, i = 0, nlcnt = 0;
        const char *uri = mg_get_request_info(conn)->local_uri;
        char *path = alloca(strlen(uri) - 7);
        strncpy(path, uri + 1, strlen(uri) - 8);
        path[strlen(uri) - 8] = '\0';
        FILE *f = fopen(path, "w");
        while ((r = mg_read(conn, buf, 2047)) > 0) {
            off = 0;
            if (i == 0) {
                while (nlcnt < 10) {
                    if (buf[off++] == '\n')
                        ++nlcnt;
                }
            }
            total += fwrite(buf + off, 1, r - off, f);
            ++i;
        }
        fflush(f);
        /* Remove trailing ----sequence */
        ftruncate(fileno(f), total - 39);
        fclose(f);
        mg_printf(conn, "HTTP/1.1 200 OK\r\n\r\n");
        mg_printf(conn, "0 - File successfully loaded in %s\n", path);
        mg_printf(conn, "destfile:%s\n", path);
        struct stat st;
        if (stat(path, &st) != -1)
            mg_printf(conn, "mtime:%ld", st.st_mtime);
        return 1;
    }
    return 0;
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
