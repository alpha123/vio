/*
    Copyright (c) 2015, Andreas Fredriksson, Micha Mettke
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
    ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*  ------------------------------------------------------------------------------
    To use this file do:
    #define WBY_IMPLEMENTATION
    before you include this file in *one* C or C++ file to create the implementation
    If you want to keep the implementation in that file you have to do
    #define WBY_STATIC befor including this file
    ------------------------------------------------------------------------------
*/
#ifndef WBY_H_
#define WBY_H_

 /* ===============================================================
 *
 *                          HEADER
 *
 * =============================================================== */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef WBY_STATIC
#define WBY_API static
#else
#define WBY_API extern
#endif

#ifdef WBY_USE_FIXED_TYPES
/* setting this define adds header <stdint.h> for fixed sized types
 * if not wanted each type has to be set to the correct size*/
#include <stdint.h>
typedef int16_t wby_short;
typedef int32_t wby_int;
typedef int32_t wby_bool;
typedef int64_t wby_long;
typedef uint16_t wby_ushort;
typedef uint32_t wby_uint;
typedef uint64_t wby_ulong;
typedef uint8_t wby_byte;
typedef uint64_t wby_size;
typedef uint64_t wby_ptr;
#else
typedef short wby_short;
typedef int wby_int;
typedef int wby_bool;
typedef long wby_long;
typedef unsigned short wby_ushort;
typedef unsigned int wby_uint;
typedef unsigned long wby_ulong;
typedef unsigned char wby_byte;
typedef unsigned long wby_size;
typedef unsigned long wby_ptr;
#endif

#define WBY_OK (0)
#define WBY_FLAG(x) (1 << (x))

#ifndef WBY_MAX_HEADERS
#define WBY_MAX_HEADERS 64
#endif

struct wby_header {
    const char *name;
    const char *value;
};

/* A HTTP request. */
struct wby_request {
    const char *method;
    /* The method of the request, e.g. "GET", "POST" and so on */
    const char *uri;
    /* The URI that was used. */
    const char *http_version;
    /* The used HTTP version */
    const char *query_params;
    /* The query parameters passed in the URL, or NULL if none were passed. */
    wby_int content_length;
    /* The number of bytes of request body that are available via WebbyRead() */
    wby_int header_count;
    /* The number of headers */
    struct wby_header headers[WBY_MAX_HEADERS];
    /* Request headers */
};

/* Connection state, as published to the serving callback. */
struct wby_con {
    struct wby_request request;
    /* The request being served. Read-only. */
    void *user_data;
    /* User data. Read-write. wby doesn't care about this. */
};

struct wby_frame {
    wby_byte flags;
    wby_byte opcode;
    wby_byte header_size;
    wby_byte padding_;
    wby_byte mask_key[4];
    wby_int payload_length;
};

enum wby_websock_flags {
    WBY_WSF_FIN    = WBY_FLAG(0),
    WBY_WSF_MASKED = WBY_FLAG(1)
};

enum wby_websock_operation {
    WBY_WSOP_CONTINUATION    = 0,
    WBY_WSOP_TEXT_FRAME      = 1,
    WBY_WSOP_BINARY_FRAME    = 2,
    WBY_WSOP_CLOSE           = 8,
    WBY_WSOP_PING            = 9,
    WBY_WSOP_PONG            = 10
};

/* Configuration data required for starting a server. */
typedef void(*wby_log_f)(const char *msg);
struct wby_config {
    void *userdata;
    /* userdata which will be passed */
    const char *address;
    /* The bind address. Must be a textual IP address. */
    wby_ushort port;
    /* The port to listen to. */
    wby_uint connection_max;
    /* Maximum number of simultaneous connections. */
    wby_size request_buffer_size;
    /* The size of the request buffer. This must be big enough to contain all
    * headers and the request line sent by the client. 2-4k is a good size for
    * this buffer. */
    wby_size io_buffer_size;
    /* The size of the I/O buffer, used when writing the reponse. 4k is a good
    * choice for this buffer.*/
    wby_log_f log;
    /* Optional callback function that receives debug log text (without newlines). */
    wby_int(*dispatch)(struct wby_con *con, void *userdata);
    /* Request dispatcher function. This function is called when the request
    * structure is ready.
    * If you decide to handle the request, call wby_response_begin(),
    * wby_write() and wby_response_end() and then return 0. Otherwise, return a
    * non-zero value to have Webby send back a 404 response. */
    wby_int(*ws_connect)(struct wby_con*, void *userdata);
    /*WebSocket connection dispatcher. Called when an incoming request wants to
    * update to a WebSocket connection.
    * Return 0 to allow the connection.
    * Return 1 to ignore the connection.*/
    void (*ws_connected)(struct wby_con*, void *userdata);
    /* Called when a WebSocket connection has been established.*/
    void (*ws_closed)(struct wby_con*, void *userdata);
    /*Called when a WebSocket connection has been closed.*/
    int (*ws_frame)(struct wby_con*, const struct wby_frame *frame, void *userdata);
    /*Called when a WebSocket data frame is incoming.
    * Call wby_read() to read the payload data.
    * Return non-zero to close the connection.*/
};

struct wby_connection;
struct wby_server {
    struct wby_config config;
    wby_size memory_size;
    wby_ptr socket;
    wby_size con_count;
    struct wby_connection *con;
};

WBY_API void wby_server_init(struct wby_server*, const struct wby_config*,
                            wby_size *needed_memory);
/*  this function clears the server and calculates the needed memory to run
    Input:
    -   filled server configuration data to calculate the needed memory
    Output:
    -   needed memory for the server to run
*/
WBY_API wby_int wby_server_start(struct wby_server*, void *memory);
/*  this function starts running the server in the specificed memory space. Size
 *  must be at least big enough as determined in the wby_server_init().
    Input:
    -   allocated memory space to create the server into
*/
WBY_API void wby_server_update(struct wby_server*);
/* updates the server by being called frequenctly (at least once a frame) */
WBY_API void wby_server_stop(struct wby_server*);
/* stops and shutdown the server */
WBY_API wby_int wby_response_begin(struct wby_con*, wby_int status_code, wby_int content_length,
                                    const struct wby_header headers[], wby_int header_count);
/*  this function begins a response
    Input:
    -   HTTP status code to send. (Normally 200).
    -   size in bytes you intend to write, or -1 for chunked encoding
    -   array of HTTP headers to transmit (can be NULL of header_count == 0)
    -   number of headers in the array
    Output:
    -   returns 0 on success, non-zero on error.
*/
WBY_API void wby_response_end(struct wby_con*);
/*  this function finishes a response. When you're done wirting the response
 *  body, call this function. this makes sure chunked encoding is terminated
 *  correctly and that the connection is setup for reuse. */
WBY_API wby_int wby_read(struct wby_con*, void *ptr, wby_size len);
/*  this function reads data from the request body. Only read what the client
 *  has priovided (via content_length) parameter, or you will end up blocking
 *  forever.
    Input:
    - pointer to a memory block that will be filled
    - size of the memory block to fill
*/
WBY_API wby_int wby_write(struct wby_con*, const void *ptr, wby_size len);
/*  this function writes a response data to the connection. If you're not using
 *  chunked encoding, be careful not to send more than the specified content
 *  length. You can call this function multiple times as long as the total
 *  number of bytes matches up with the content length.
    Input:
    - pointer to a memory block that will be send
    - size of the memory block to send
*/
WBY_API wby_int wby_frame_begin(struct wby_con*, wby_int opcode);
/*  this function begins an outgoing websocket frame */
WBY_API wby_int wby_frame_end(struct wby_con*);
/*  this function finishes an outgoing websocket frame */
WBY_API wby_int wby_find_query_var(const char *buf, const char *name, char *dst, wby_size dst_len);
/*  this function is a helper function to lookup a query parameter given a URL
 *  encoded string. Returns the size of the returned data, or -1 if the query
 *  var wasn't found. */
WBY_API const char* wby_find_header(struct wby_con*, const char *name);
/*  this convenience function to find a header in a request. Returns the value
 *  of the specified header, or NULL if its was not present. */

#ifdef __cplusplus
}
#endif
#endif /* WBY_H_ */
/* ===============================================================
 *
 *                      IMPLEMENTATION
 *
 * ===============================================================*/
#ifdef WBY_IMPLEMENTATION
#define WBY_LEN(a) (sizeof(a)/sizeof((a)[0]))
#define WBY_UNUSED(a) ((void)(a))

#ifdef WBY_USE_ASSERT
#ifndef WBY_ASSERT
#include <assert.h>
#define WBY_ASSERT(expr) assert(expr)
#endif
#else
#define WBY_ASSERT(expr)
#endif

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#define WBY_SOCK(s) ((wby_socket)(s))
#define WBY_INTERN static
#define WBY_GLOBAL static
#define WBY_STORAGE static
/* ===============================================================
 *                          UTIL
 * ===============================================================*/
struct wby_buffer {
    wby_size used;
    /* current buffer size */
    wby_size max;
    /* buffer capacity */
    wby_byte *data;
    /* pointer inside a global buffer */
};

WBY_INTERN void
wby_dbg(wby_log_f log, const char *fmt, ...)
{
    char buffer[1024];
    va_list args;
    if (!log) return;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof buffer, fmt, args);
    va_end(args);
    buffer[(sizeof buffer)-1] = '\0';
    log(buffer);
}

WBY_INTERN wby_int
wb_peek_request_size(const wby_byte *buf, wby_int len)
{
    wby_int i;
    wby_int max = len - 3;
    for (i = 0; i < max; ++i) {
        if ('\r' != buf[i + 0]) continue;
        if ('\n' != buf[i + 1]) continue;
        if ('\r' != buf[i + 2]) continue;
        if ('\n' != buf[i + 3]) continue;
        /* OK; we have CRLFCRLF which indicates the end of the header section */
        return i + 4;
    }
    return -1;
}

WBY_INTERN char*
wby_skipws(char *p)
{
    for (;;) {
        char ch = *p;
        if (' ' == ch || '\t' == ch) ++p;
        else break;
    }
    return p;
}

#define WBY_TOK_SKIPWS WBY_FLAG(0)
WBY_INTERN wby_int
wby_tok_inplace(char *buf, const char* separator, char *tokens[], wby_int max, wby_int flags)
{
    char *b = buf;
    char *e = buf;
    wby_int token_count = 0;
    wby_int separator_len = (wby_int)strlen(separator);
    while (token_count < max) {
        if (flags & WBY_TOK_SKIPWS)
            b = wby_skipws(b);
        if (NULL != (e = strstr(b, separator))) {
            int len = (wby_int) (e - b);
            if (len > 0)
                tokens[token_count++] = b;
            *e = '\0';
            b = e + separator_len;
        } else {
            tokens[token_count++] = b;
            break;
        }
    }
    return token_count;
}

WBY_INTERN wby_size
wby_make_websocket_header(wby_byte buffer[10], wby_byte opcode, wby_int payload_len, wby_int fin)
{
    buffer[0] = (wby_byte)((fin ? 0x80 : 0x00) | opcode);
    if (payload_len < 126) {
        buffer[1] = (wby_byte)(payload_len & 0x7f);
        return 2;
    } else if (payload_len < 65536) {
        buffer[1] = 126;
        buffer[2] = (wby_byte)(payload_len >> 8);
        buffer[3] = (wby_byte)payload_len;
        return 4;
    } else {
        buffer[1] = 127;
        /* Ignore high 32-bits. I didn't want to require 64-bit types and typdef hell in the API. */
        buffer[2] = buffer[3] = buffer[4] = buffer[5] = 0;
        buffer[6] = (wby_byte)(payload_len >> 24);
        buffer[7] = (wby_byte)(payload_len >> 16);
        buffer[8] = (wby_byte)(payload_len >> 8);
        buffer[9] = (wby_byte)payload_len;
        return 10;
    }
}

WBY_INTERN wby_int
wby_read_buffered_data(int *data_left, struct wby_buffer* buffer,
    char **dest_ptr, wby_size *dest_len)
{
    wby_int offset, read_size;
    wby_int left = *data_left;
    wby_int len;
    if (left == 0)
        return 0;

    len = (wby_int) *dest_len;
    offset = (wby_int)buffer->used - left;
    read_size = (len > left) ? left : len;
    memcpy(*dest_ptr, buffer->data + offset, (wby_size)read_size);

    (*dest_ptr) += read_size;
    (*dest_len) -= (wby_size) read_size;
    (*data_left) -= read_size;
    return read_size;
}
/* ===============================================================
 *                          SOCKET
 * ===============================================================*/
#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET wby_socket;
typedef wby_int wby_socklen;

#if defined(__GNUC__)
#define WBY_ALIGN(x) __attribute__((aligned(x)))
#else
#define WBY_ALIGN(x) __declspec(align(x))
#endif

#define WBY_INVALID_SOCKET INVALID_SOCKET
#define snprintf _snprintf

WBY_INTERN wby_int
wby_socket_error(void)
{
    return WSAGetLastError();
}

#if !defined(__GNUC__)
WBY_INTERN wby_int
strcasecmp(const char *a, const char *b)
{
    return _stricmp(a, b);
}

WBY_INTERN wby_int
strncasecmp(const char *a, const char *b, wby_size len)
{
    return _strnicmp(a, b, len);
}
#endif

WBY_INTERN wby_int
wby_socket_set_blocking(wby_socket socket, wby_int blocking)
{
    u_long val = !blocking;
    return ioctlsocket(socket, FIONBIO, &val);
}

WBY_INTERN wby_int
wby_socket_is_valid(wby_socket socket)
{
    return (INVALID_SOCKET != socket);
}

WBY_INTERN void
wby_socket_close(wby_socket socket)
{
    closesocket(socket);
}

WBY_INTERN wby_int
wby_socket_is_blocking_error(wby_int error)
{
    return WSAEWOULDBLOCK == error;
}

#else /* UNIX */

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>

typedef wby_int wby_socket;
typedef socklen_t wby_socklen;

#define WBY_ALIGN(x) __attribute__((aligned(x)))
#define WBY_INVALID_SOCKET (-1)

WBY_INTERN wby_int
wby_socket_error(void)
{
    return errno;
}

WBY_INTERN wby_int
wby_socket_is_valid(wby_socket socket)
{
    return (socket > 0);
}

WBY_INTERN void
wby_socket_close(wby_socket socket)
{
    close(socket);
}

WBY_INTERN wby_int
wby_socket_is_blocking_error(wby_int error)
{
    return (EAGAIN == error);
}

WBY_INTERN wby_int
wby_socket_set_blocking(wby_socket socket, wby_int blocking)
{
    wby_int flags = fcntl(socket, F_GETFL, 0);
    if (flags < 0) return flags;
    flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    return fcntl(socket, F_SETFL, flags);
}
#endif

WBY_INTERN wby_int
wby_socket_config_incoming(wby_socket socket)
{
    wby_int off = 0;
    wby_int err;
    if ((err = wby_socket_set_blocking(socket, 0)) != WBY_OK) return err;
    setsockopt(socket, SOL_SOCKET, SO_LINGER, (const char*) &off, sizeof(wby_int));
    return 0;
}

WBY_INTERN wby_int
wby_socket_send(wby_socket socket, const wby_byte *buffer, wby_int size)
{
    while (size > 0) {
        wby_long err = send(socket, (const char*)buffer, (wby_size)size, 0);
        if (err <= 0) return 1;
        buffer += err;
        size -= (wby_int)err;
    }
    return 0;
}

/* Read as much as possible without blocking while there is buffer space. */
enum {WBY_FILL_OK, WBY_FILL_ERROR, WBY_FILL_FULL};
WBY_INTERN wby_int
wby_socket_recv(wby_socket socket, struct wby_buffer *buf, wby_log_f log)
{
    wby_long err;
    wby_int buf_left;
    for (;;) {
        buf_left = (wby_int)buf->max - (wby_int)buf->used;
        wby_dbg(log, "buffer space left = %d", buf_left);
        if (buf_left == 0)
            return WBY_FILL_FULL;

        /* Read what we can into the current buffer space. */
        err = recv(socket, (char*)buf->data + buf->used, (wby_size)buf_left, 0);
        if (err < 0) {
            wby_int sock_err = wby_socket_error();
            if (wby_socket_is_blocking_error(sock_err)) {
                return WBY_FILL_OK;
            } else {
                /* Read error. Give up. */
                wby_dbg(log, "read error %d - connection dead", sock_err);
                return WBY_FILL_ERROR;
            }
        } else if (err == 0) {
          /* The peer has closed the connection. */
          wby_dbg(log, "peer has closed the connection");
          return WBY_FILL_ERROR;
        } else buf->used += (wby_size)err;
    }
}

WBY_INTERN wby_int
wby_socket_flush(wby_socket socket, struct wby_buffer *buf)
{
    if (buf->used > 0){
        if (wby_socket_send(socket, buf->data, (wby_int)buf->used) != WBY_OK)
            return 1;
    }
    buf->used = 0;
    return 0;
}
/* ===============================================================
 *                          URL
 * ===============================================================*/

/* URL-decode input buffer into destination buffer.
 * 0-terminate the destination buffer. Return the length of decoded data.
 * form-url-encoded data differs from URI encoding in a way that it
 * uses '+' as character for space, see RFC 1866 section 8.2.1
 * http://ftp.ics.uci.edu/pub/ietf/html/rfc1866.txt
 *
 * This bit of code was taken from mongoose.
 */
WBY_INTERN wby_size
wby_url_decode(const char *src, wby_size src_len, char *dst, wby_size dst_len,
    wby_int is_form_url_encoded)
{
    wby_int a, b;
    wby_size i, j;
    #define HEXTOI(x) (isdigit(x) ? x - '0' : x - 'W')
    for (i = j = 0; i < src_len && j < dst_len - 1; i++, j++) {
        if (src[i] == '%' &&
            isxdigit(*(const wby_byte*)(src + i + 1)) &&
            isxdigit(*(const wby_byte*)(src + i + 2)))
        {
            a = tolower(*(const wby_byte*)(src + i + 1));
            b = tolower(*(const wby_byte*)(src + i + 2));
            dst[j] = (char)((HEXTOI(a) << 4) | HEXTOI(b));
            i += 2;
        } else if (is_form_url_encoded && src[i] == '+') {
            dst[j] = ' ';
        } else dst[j] = src[i];
    }
    #undef HEXTOI
    dst[j] = '\0'; /* Null-terminate the destination */
    return j;
}

/* Pulled from mongoose */
WBY_API wby_int
wby_find_query_var(const char *buf, const char *name, char *dst, wby_size dst_len)
{
    const char *p, *e, *s;
    wby_size name_len;
    wby_int len;
    wby_size buf_len = strlen(buf);

    name_len = strlen(name);
    e = buf + buf_len;
    len = -1;
    dst[0] = '\0';

    /* buf is "var1=val1&var2=val2...". Find variable first */
    for (p = buf; p != NULL && p + name_len < e; p++) {
        if ((p == buf || p[-1] == '&') && p[name_len] == '=' &&
            strncasecmp(name, p, name_len) == 0)
        {
            /* Point p to variable value */
            p += name_len + 1;
            /* Point s to the end of the value */
            s = (const char *) memchr(p, '&', (wby_size)(e - p));
            if (s == NULL) s = e;
            WBY_ASSERT(s >= p);
            /* Decode variable into destination buffer */
            if ((wby_size) (s - p) < dst_len)
                len = (wby_int)wby_url_decode(p, (wby_size)(s - p), dst, dst_len, 1);
            break;
        }
    }
    return len;
}
/* ===============================================================
 *                          BASE64
 * ===============================================================*/
#define WBY_BASE64_QUADS_BEFORE_LINEBREAK 19

WBY_INTERN wby_size
wby_base64_bufsize(wby_size input_size)
{
    wby_size triplets = (input_size + 2) / 3;
    wby_size base_size = 4 * triplets;
    wby_size line_breaks = 2 * (triplets / WBY_BASE64_QUADS_BEFORE_LINEBREAK);
    wby_size null_termination = 1;
    return base_size + line_breaks + null_termination;
}

WBY_INTERN wby_int
wby_base64_encode(char* output, wby_size output_size,
    const wby_byte *input, wby_size input_size)
{
    wby_size i = 0;
    wby_int line_out = 0;
    WBY_STORAGE const char enc[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/=";
    if (output_size < wby_base64_bufsize(input_size))
        return 1;

    while (i < input_size) {
        wby_uint idx_0, idx_1, idx_2, idx_3;
        wby_uint i0;

        i0 = (wby_uint)(input[i]) << 16; i++;
        i0 |= (wby_uint)(i < input_size ? input[i] : 0) << 8; i++;
        i0 |= (i < input_size ? input[i] : 0); i++;

        idx_0 = (i0 & 0xfc0000) >> 18; i0 <<= 6;
        idx_1 = (i0 & 0xfc0000) >> 18; i0 <<= 6;
        idx_2 = (i0 & 0xfc0000) >> 18; i0 <<= 6;
        idx_3 = (i0 & 0xfc0000) >> 18;

        if (i - 1 > input_size) idx_2 = 64;
        if (i > input_size) idx_3 = 64;

        *output++ = enc[idx_0];
        *output++ = enc[idx_1];
        *output++ = enc[idx_2];
        *output++ = enc[idx_3];

        if (++line_out == WBY_BASE64_QUADS_BEFORE_LINEBREAK) {
          *output++ = '\r';
          *output++ = '\n';
        }
    }
    *output = '\0';
    return 0;
}
/* ===============================================================
 *                          SHA1
 * ===============================================================*/
struct wby_sha1 {
    wby_uint state[5];
    wby_uint msg_size[2];
    wby_size buf_used;
    wby_byte buffer[64];
};

WBY_INTERN wby_uint
wby_sha1_rol(wby_uint value, wby_uint bits)
{
    return ((value) << bits) | (value >> (32 - bits));
}

WBY_INTERN void
wby_sha1_hash_block(wby_uint state[5], const wby_byte *block)
{
    wby_int i;
    wby_uint a, b, c, d, e;
    wby_uint w[80];

    /* Prepare message schedule */
    for (i = 0; i < 16; ++i) {
        w[i] =  (((wby_uint)block[(i*4)+0]) << 24) |
                (((wby_uint)block[(i*4)+1]) << 16) |
                (((wby_uint)block[(i*4)+2]) <<  8) |
                (((wby_uint)block[(i*4)+3]) <<  0);
    }

    for (i = 16; i < 80; ++i)
        w[i] = wby_sha1_rol(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    /* Initialize working variables */
    a = state[0]; b = state[1]; c = state[2]; d = state[3]; e = state[4];

    /* This is the core loop for each 20-word span. */
    #define SHA1_LOOP(start, end, func, constant) \
        for (i = (start); i < (end); ++i) { \
            wby_uint t = wby_sha1_rol(a, 5) + (func) + e + (constant) + w[i]; \
            e = d; d = c; c = wby_sha1_rol(b, 30); b = a; a = t;}

    SHA1_LOOP( 0, 20, ((b & c) ^ (~b & d)),           0x5a827999)
    SHA1_LOOP(20, 40, (b ^ c ^ d),                    0x6ed9eba1)
    SHA1_LOOP(40, 60, ((b & c) ^ (b & d) ^ (c & d)),  0x8f1bbcdc)
    SHA1_LOOP(60, 80, (b ^ c ^ d),                    0xca62c1d6)
    #undef SHA1_LOOP

    /* Update state */
    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

WBY_INTERN void
wby_sha1_init(struct wby_sha1 *s)
{
    s->state[0] = 0x67452301;
    s->state[1] = 0xefcdab89;
    s->state[2] = 0x98badcfe;
    s->state[3] = 0x10325476;
    s->state[4] = 0xc3d2e1f0;
    s->msg_size[0] = 0;
    s->msg_size[1] = 0;
    s->buf_used = 0;
}

WBY_INTERN void
wby_sha1_update(struct wby_sha1 *s, const void *data_, wby_size size)
{
    const char *data = (const char*)data_;
    wby_uint size_lo;
    wby_uint size_lo_orig;
    wby_size remain = size;

    while (remain > 0) {
        wby_size buf_space = sizeof(s->buffer) - s->buf_used;
        wby_size copy_size = (remain < buf_space) ? remain : buf_space;
        memcpy(s->buffer + s->buf_used, data, copy_size);
        s->buf_used += copy_size;
        data += copy_size;
        remain -= copy_size;

        if (s->buf_used == sizeof(s->buffer)) {
            wby_sha1_hash_block(s->state, s->buffer);
            s->buf_used = 0;
        }
    }

    size_lo = size_lo_orig = s->msg_size[1];
    size_lo += (wby_uint)(size * 8);
    if (size_lo < size_lo_orig)
        s->msg_size[0] += 1;
    s->msg_size[1] = size_lo;
}

WBY_INTERN void
wby_sha1_final(wby_byte digest[20], struct wby_sha1 *s)
{
    wby_byte zero = 0x00;
    wby_byte one_bit = 0x80;
    wby_byte count_data[8];
    wby_int i;

    /* Generate size data in bit endian format */
    for (i = 0; i < 8; ++i) {
        wby_uint word = s->msg_size[i >> 2];
        count_data[i] = (wby_byte)(word >> ((3 - (i & 3)) * 8));
    }

    /* Set trailing one-bit */
    wby_sha1_update(s, &one_bit, 1);
    /* Emit null padding to to make room for 64 bits of size info in the last 512 bit block */
    while (s->buf_used != 56)
        wby_sha1_update(s, &zero, 1);

    /* Write size in bits as last 64-bits */
    wby_sha1_update(s, count_data, 8);
    /* Make sure we actually finalized our last block */
    WBY_ASSERT(s->buf_used == 0);

    /* Generate digest */
    for (i = 0; i < 20; ++i) {
        wby_uint word = s->state[i >> 2];
        wby_byte byte = (wby_byte) ((word >> ((3 - (i & 3)) * 8)) & 0xff);
        digest[i] = byte;
    }
}
/* ===============================================================
 *                          CONNECTION
 * ===============================================================*/
#define WBY_WEBSOCKET_VERSION "13"
WBY_GLOBAL const char wby_continue_header[] = "HTTP/1.1 100 Continue\r\n\r\n";
WBY_GLOBAL const wby_size wby_continue_header_len = sizeof(wby_continue_header) - 1;
WBY_GLOBAL const char wby_websocket_guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
WBY_GLOBAL const wby_size wby_websocket_guid_len = sizeof(wby_websocket_guid) - 1;
WBY_GLOBAL const wby_byte wby_websocket_pong[] = { 0x80, WBY_WSOP_PONG, 0x00 };
WBY_GLOBAL const struct wby_header wby_plain_text_headers[]={{"Content-Type","text/plain"}};

enum wby_connection_flags {
    WBY_CON_ALIVE               = WBY_FLAG(0),
    WBY_CON_FRESH_CONNECTION     = WBY_FLAG(1),
    WBY_CON_CLOSE_AFTER_RESPONSE = WBY_FLAG(2),
    WBY_CON_CHUNKED_RESPONSE     = WBY_FLAG(3),
    WBY_CON_WEBSOCKET            = WBY_FLAG(4)
};

enum wby_connection_state {
    WBY_REQUEST,
    WBY_SEND_CONTINUE,
    WBY_SERVE,
    WBY_WEBSOCKET
};

struct wby_connection {
    struct wby_con public_data;
    wby_ushort flags;
    wby_ushort state;
    wby_ptr socket;
    wby_log_f log;
    wby_size request_buffer_size;
    wby_size io_buffer_size;
    struct wby_buffer header_buf;
    struct wby_buffer io_buf;
    wby_int header_body_left;
    wby_int io_data_left;
    wby_int continue_data_left;
    wby_int body_bytes_read;
    struct wby_frame ws_frame;
    wby_byte ws_opcode;
    wby_size blocking_count;
};

WBY_INTERN wby_int
wby_connection_set_blocking(struct wby_connection *conn)
{
    if (conn->blocking_count == 0) {
        if (wby_socket_set_blocking(WBY_SOCK(conn->socket), 1) != WBY_OK) {
            wby_dbg(conn->log, "failed to switch connection to blocking");
            conn->flags &= (wby_ushort)~WBY_CON_ALIVE;
            return -1;
        }
    }
    ++conn->blocking_count;
    return 0;
}

WBY_INTERN wby_int
wby_connection_set_nonblocking(struct wby_connection *conn)
{
    wby_size count = conn->blocking_count;
    if (count == 1) {
        if (wby_socket_set_blocking(WBY_SOCK(conn->socket), 0) != WBY_OK) {
            wby_dbg(conn->log, "failed to switch connection to non-blocking");
            conn->flags &= (wby_ushort)~WBY_CON_ALIVE;
            return -1;
        }
    }
    conn->blocking_count = count - 1;
    return 0;
}

WBY_INTERN void
wby_connection_reset(struct wby_connection *conn, wby_size request_buffer_size,
    wby_size io_buffer_size)
{
    conn->header_buf.used = 0;
    conn->header_buf.max = request_buffer_size;
    conn->io_buf.used = 0;
    conn->io_buf.max = io_buffer_size;
    conn->header_body_left = 0;
    conn->io_data_left = 0;
    conn->continue_data_left = 0;
    conn->body_bytes_read = 0;
    conn->state = WBY_REQUEST;
    conn->public_data.user_data = NULL;
    conn->blocking_count = 0;
}

WBY_INTERN void
wby_connection_close(struct wby_connection* connection)
{
    if (WBY_SOCK(connection->socket) != WBY_INVALID_SOCKET) {
        wby_socket_close(WBY_SOCK(connection->socket));
        connection->socket = (wby_ptr)WBY_INVALID_SOCKET;
    }
    connection->flags = 0;
}

WBY_INTERN wby_int
wby_connection_setup_request(struct wby_connection *connection, wby_int request_size)
{
    char* lines[WBY_MAX_HEADERS + 2];
    wby_int line_count;
    char* tok[16];
    char* query_params;
    wby_int tok_count;

    wby_int i;
    wby_int header_count;
    char *buf = (char*) connection->header_buf.data;
    struct wby_request *req = &connection->public_data.request;

    /* Null-terminate the request envelope by overwriting the last CRLF with 00LF */
    buf[request_size - 2] = '\0';
    /* Split header into lines */
    line_count = wby_tok_inplace(buf, "\r\n", lines, WBY_LEN(lines), 0);
    header_count = line_count - 2;
    if (line_count < 1 || header_count > (wby_int) WBY_LEN(req->headers))
        return 1;

    /* Parse request line */
    tok_count = wby_tok_inplace(lines[0], " ", tok, WBY_LEN(tok), 0);
    if (3 != tok_count) return 1;

    req->method = tok[0];
    req->uri = tok[1];
    req->http_version = tok[2];
    req->content_length = 0;

    /* See if there are any query parameters */
    if ((query_params = (char*) strchr(req->uri, '?')) != NULL) {
        req->query_params = query_params + 1;
        *query_params = '\0';
    } else req->query_params = NULL;

    {
        /* Decode the URI in place */
        wby_size uri_len = strlen(req->uri);
        wby_url_decode(req->uri, uri_len, (char*)req->uri, uri_len + 1, 1);
    }

    /* Parse headers */
    for (i = 0; i < header_count; ++i) {
        tok_count = wby_tok_inplace(lines[i + 1], ":", tok, 2, WBY_TOK_SKIPWS);
        if (tok_count != 2) return 1;
        req->headers[i].name = tok[0];
        req->headers[i].value = tok[1];

        if (!strcasecmp("content-length", tok[0])) {
            req->content_length = (wby_int)strtoul(tok[1], NULL, 10);
            wby_dbg(connection->log, "request has body; content length is %d", req->content_length);
        } else if (!strcasecmp("transfer-encoding", tok[0])) {
            wby_dbg(connection->log, "cowardly refusing to handle Transfer-Encoding: %s", tok[1]);
            return 1;
        }
    }
    req->header_count = header_count;
    return 0;
}

WBY_INTERN wby_int
wby_connection_send_websocket_upgrade(struct wby_connection* connection)
{
    const char *hdr;
    struct wby_sha1 sha;
    wby_byte digest[20];
    char output_digest[64];
    struct wby_header headers[3];
    struct wby_con *conn = &connection->public_data;
    if ((hdr = wby_find_header(conn, "Sec-WebSocket-Version")) == NULL) {
        wby_dbg(connection->log, "Sec-WebSocket-Version header not present");
        return 1;
    }
    if (strcmp(hdr, WBY_WEBSOCKET_VERSION)) {
        wby_dbg(connection->log,"WebSocket version %s not supported (we only do %s)",
                hdr, WBY_WEBSOCKET_VERSION);
        return 1;
    }
    if ((hdr = wby_find_header(conn, "Sec-WebSocket-Key")) == NULL) {
        wby_dbg(connection->log, "Sec-WebSocket-Key header not present");
        return 1;
    }
    /* Compute SHA1 hash of Sec-Websocket-Key + the websocket guid as required by
    * the RFC.
    *
    * This handshake is bullshit. It adds zero security. Just forces me to drag
    * in SHA1 and create a base64 encoder.
    */
    wby_sha1_init(&sha);
    wby_sha1_update(&sha, hdr, strlen(hdr));
    wby_sha1_update(&sha, wby_websocket_guid, wby_websocket_guid_len);
    wby_sha1_final(&digest[0], &sha);
    if (wby_base64_encode(output_digest, sizeof output_digest, &digest[0], sizeof(digest)) != WBY_OK)
        return 1;

    headers[0].name  = "Upgrade";
    headers[0].value = "websocket";
    headers[1].name  = "Connection";
    headers[1].value = "Upgrade";
    headers[2].name  = "Sec-WebSocket-Accept";
    headers[2].value = output_digest;
    wby_response_begin(&connection->public_data, 101, 0, headers, WBY_LEN(headers));
    wby_response_end(&connection->public_data);
    return 0;
}

WBY_INTERN wby_int
wby_connection_push(struct wby_connection *conn, const void *data_, wby_int len)
{
    struct wby_buffer *buf = &conn->io_buf;
    const wby_byte* data = (const wby_byte*)data_;
    if (conn->state != WBY_SERVE) {
        wby_dbg(conn->log, "attempt to write in non-serve state");
        return 1;
    }
    if (len == 0)
        return wby_socket_flush(WBY_SOCK(conn->socket), buf);

    while (len > 0) {
        wby_int buf_space = (wby_int)buf->max - (wby_int)buf->used;
        wby_int copy_size = len < buf_space ? len : buf_space;
        memcpy(buf->data + buf->used, data, (wby_size)copy_size);
        buf->used += (wby_size)copy_size;

        data += copy_size;
        len -= copy_size;
        if (buf->used == buf->max) {
            if (wby_socket_flush(WBY_SOCK(conn->socket), buf) != WBY_OK)
                return 1;
            if ((wby_size)len >= buf->max)
                return wby_socket_send(WBY_SOCK(conn->socket), data, len);
        }
    }
    return 0;
}
/* ===============================================================
 *                          CON/REQUEST
 * ===============================================================*/
WBY_INTERN wby_int
wby_con_discard_incoming_data(struct wby_con* conn, wby_int count)
{
    while (count > 0) {
        char buffer[1024];
        wby_int read_size = (wby_int)(((wby_size)count > sizeof(buffer)) ?
            sizeof(buffer) : (wby_size)count);
        if (wby_read(conn, buffer, (wby_size)read_size) != WBY_OK)
            return -1;
        count -= read_size;
    }
    return 0;
}

WBY_API const char*
wby_find_header(struct wby_con *conn, const char *name)
{
    wby_int i, count;
    for (i = 0, count = conn->request.header_count; i < count; ++i) {
        if (!strcasecmp(conn->request.headers[i].name, name))
            return conn->request.headers[i].value;
    }
    return NULL;
}

WBY_INTERN wby_int
wby_con_is_websocket_request(struct wby_con* conn)
{
    const char *hdr;
    if ((hdr = wby_find_header(conn, "Connection")) == NULL) return 0;
    /*if (strcasecmp(hdr, "Upgrade")) return 0;*/
    if ((hdr = wby_find_header(conn, "Upgrade")) == NULL) return 0;
    /*if (strcasecmp(hdr, "websocket")) return 0;*/
    return 1;
}

WBY_INTERN wby_int
wby_scan_websocket_frame(struct wby_frame *frame, const struct wby_buffer *buf)
{
    wby_byte flags = 0;
    wby_uint len = 0;
    wby_uint opcode = 0;
    wby_byte* data = buf->data;
    wby_byte* data_max = data + buf->used;

    wby_int i;
    wby_int len_bytes = 0;
    wby_int mask_bytes = 0;
    wby_byte header0, header1;
    if (buf->used < 2)
        return -1;

    header0 = *data++;
    header1 = *data++;
    if (header0 & 0x80)
        flags |= WBY_WSF_FIN;
    if (header1 & 0x80) {
        flags |= WBY_WSF_MASKED;
        mask_bytes = 4;
    }

    opcode = header0 & 0xf;
    len = header1 & 0x7f;
    if (len == 126)
        len_bytes = 2;
    else if (len == 127)
        len_bytes = 8;
    if (data + len_bytes + mask_bytes > data_max)
        return -1;

    /* Read big endian length from length bytes (if greater than 125) */
    len = len_bytes == 0 ? len : 0;
    for (i = 0; i < len_bytes; ++i) {
        /* This will totally overflow for 64-bit values. I don't care.
         * If you're transmitting more than 4 GB of data using Webby,
         * seek help. */
        len <<= 8;
        len |= *data++;
    }

    /* Read mask word if present */
    for (i = 0; i < mask_bytes; ++i)
        frame->mask_key[i] = *data++;
    frame->header_size = (wby_byte) (data - buf->data);
    frame->flags = flags;
    frame->opcode = (wby_byte) opcode;
    frame->payload_length = (wby_int)len;
    return 0;
}

WBY_API wby_int
wby_frame_begin(struct wby_con *conn_pub, wby_int opcode)
{
    struct wby_connection *conn = (struct wby_connection*)conn_pub;
    conn->ws_opcode = (wby_byte) opcode;
    /* Switch socket to blocking mode */
    return wby_connection_set_blocking(conn);
}

WBY_API wby_int
wby_frame_end(struct wby_con *conn_pub)
{
    wby_byte header[10];
    wby_size header_size;
    struct wby_connection *conn = (struct wby_connection*) conn_pub;
    header_size = wby_make_websocket_header(header, conn->ws_opcode, 0, 1);
    if (wby_socket_send(WBY_SOCK(conn->socket), header, (wby_int) header_size) != WBY_OK)
        conn->flags &= (wby_ushort)~WBY_CON_ALIVE;
    /* Switch socket to non-blocking mode */
    return wby_connection_set_nonblocking(conn);
}

WBY_API wby_int
wby_read(struct wby_con *conn, void *ptr_, wby_size len)
{
    struct wby_connection* conn_prv = (struct wby_connection*)conn;
    char *ptr = (char*) ptr_;
    wby_int count;

    wby_int start_pos = conn_prv->body_bytes_read;
    if (conn_prv->header_body_left > 0) {
        count = wby_read_buffered_data(&conn_prv->header_body_left, &conn_prv->header_buf, &ptr, &len);
        conn_prv->body_bytes_read += count;
    }

    /* Read buffered websocket data */
    if (conn_prv->io_data_left > 0) {
        count = wby_read_buffered_data(&conn_prv->io_data_left, &conn_prv->io_buf, &ptr, &len);
        conn_prv->body_bytes_read += count;
    }

    while (len > 0) {
        wby_long err = recv(WBY_SOCK(conn_prv->socket), ptr, (wby_size)len, 0);
        if (err < 0) {
            conn_prv->flags &= (wby_ushort)~WBY_CON_ALIVE;
            return (wby_int)err;
        }
        len -= (wby_size)err;
        ptr += (wby_size)err;
        conn_prv->body_bytes_read += (wby_int)err;
    }

    if ((conn_prv->flags & WBY_CON_WEBSOCKET) && (conn_prv->ws_frame.flags & WBY_WSF_MASKED)) {
        /* XOR outgoing data with websocket ofuscation key */
        wby_int i, end_pos = conn_prv->body_bytes_read;
        const wby_byte *mask = conn_prv->ws_frame.mask_key;
        ptr = (char*) ptr_; /* start over */
        for (i = start_pos; i < end_pos; ++i) {
            wby_byte byte = (wby_byte)*ptr;
            *ptr++ = (char)(byte ^ mask[i & 3]);
        }
    }
    return 0;
}

WBY_API wby_int
wby_write(struct wby_con *conn, const void *ptr, wby_size len)
{
    struct wby_connection *conn_priv = (struct wby_connection*) conn;
    if (conn_priv->flags & WBY_CON_WEBSOCKET) {
        wby_byte header[10];
        wby_size header_size;
        header_size = wby_make_websocket_header(header, conn_priv->ws_opcode, (wby_int)len, 0);

        /* Overwrite opcode to be continuation packages from here on out */
        conn_priv->ws_opcode = WBY_WSOP_CONTINUATION;
        if (wby_socket_send(WBY_SOCK(conn_priv->socket), header, (wby_int)header_size) != WBY_OK) {
            conn_priv->flags &= (wby_ushort)~WBY_CON_ALIVE;
            return -1;
        }
        if (wby_socket_send(WBY_SOCK(conn_priv->socket),(const wby_byte*)ptr, (wby_int)len) != WBY_OK) {
            conn_priv->flags &= (wby_ushort)~WBY_CON_ALIVE;
            return -1;
        }
        return 0;
    } else if (conn_priv->flags & WBY_CON_CHUNKED_RESPONSE) {
        char chunk_header[128];
        wby_int header_len = snprintf(chunk_header, sizeof chunk_header, "%x\r\n", (wby_int) len);
        wby_connection_push(conn_priv, chunk_header, header_len);
        wby_connection_push(conn_priv, ptr, (wby_int)len);
        return wby_connection_push(conn_priv, "\r\n", 2);
    } else return wby_connection_push(conn_priv, ptr, (wby_int) len);
}

WBY_INTERN wby_int
wby_printf(struct wby_con* conn, const char* fmt, ...)
{
    wby_int len;
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    len = vsnprintf(buffer, sizeof buffer, fmt, args);
    va_end(args);
    return wby_write(conn, buffer, (wby_size)len);
}
/* ===============================================================
 *                          RESPONSE
 * ===============================================================*/
#define WBY_STATUS_MAP(STATUS)\
    STATUS(100, "Continue")\
    STATUS(101, "Switching Protocols")\
    STATUS(200, "OK")\
    STATUS(201, "Created")\
    STATUS(202, "Accepted")\
    STATUS(203, "Non-Authoritative Information")\
    STATUS(204, "No Content")\
    STATUS(205, "Reset Content")\
    STATUS(206, "Partial Content")\
    STATUS(300, "Multiple Choices")\
    STATUS(301, "Moved Permanently")\
    STATUS(302, "Found")\
    STATUS(303, "See Other")\
    STATUS(304, "Not Modified")\
    STATUS(305, "Use Proxy")\
    STATUS(307, "Temporary Redirect")\
    STATUS(400, "Bad Request")\
    STATUS(401, "Unauthorized")\
    STATUS(402, "Payment Required")\
    STATUS(403, "Forbidden")\
    STATUS(404, "Not Found")\
    STATUS(405, "Method Not Allowed")\
    STATUS(406, "Not Acceptable")\
    STATUS(407, "Proxy Authentication Required")\
    STATUS(408, "Request Time-out")\
    STATUS(409, "Conflict")\
    STATUS(410, "Gone")\
    STATUS(411, "Length Required")\
    STATUS(412, "Precondition Failed")\
    STATUS(413, "Request Entity Too Large")\
    STATUS(414, "Request-URI Too Large")\
    STATUS(415, "Unsupported Media Type")\
    STATUS(416, "Requested range not satisfiable")\
    STATUS(417, "Expectation Failed")\
    STATUS(500, "Internal Server Error")\
    STATUS(501, "Not Implemented")\
    STATUS(502, "Bad Gateway")\
    STATUS(503, "Service Unavailable")\
    STATUS(504, "Gateway Time-out")\
    STATUS(505, "HTTP Version not supported")

WBY_GLOBAL const short wby_status_nums[] = {
#define WBY_STATUS(id, name) id,
    WBY_STATUS_MAP(WBY_STATUS)
#undef WBY_STATUS
};
WBY_GLOBAL const char* wby_status_text[] = {
#define WBY_STATUS(id, name) name,
    WBY_STATUS_MAP(WBY_STATUS)
#undef WBY_STATUS
};

WBY_INTERN const char*
wby_response_status_text(wby_int status_code)
{
    wby_int i;
    for (i = 0; i < (wby_int) WBY_LEN(wby_status_nums); ++i) {
        if (wby_status_nums[i] == status_code)
            return wby_status_text[i];
    }
    return "Unknown";
}

WBY_API wby_int
wby_response_begin(struct wby_con *conn_pub, wby_int status_code, wby_int content_length,
    const struct wby_header *headers, wby_int header_count)
{
    wby_int i = 0;
    struct wby_connection *conn = (struct wby_connection *)conn_pub;
    if (conn->body_bytes_read < (wby_int)conn->public_data.request.content_length) {
        wby_int body_left = conn->public_data.request.content_length - (wby_int)conn->body_bytes_read;
        if (wby_con_discard_incoming_data(conn_pub, body_left) != WBY_OK) {
            conn->flags &= (wby_ushort)~WBY_CON_ALIVE;
            return -1;
        }
    }

    wby_printf(conn_pub, "HTTP/1.1 %d %s\r\n", status_code, wby_response_status_text(status_code));
    if (content_length >= 0)
        wby_printf(conn_pub, "Content-Length: %d\r\n", content_length);
    else wby_printf(conn_pub, "Transfer-Encoding: chunked\r\n");
    wby_printf(conn_pub, "Server: Webby\r\n");

    for (i = 0; i < header_count; ++i) {
        if (!strcasecmp(headers[i].name, "Connection")) {
            if (!strcasecmp(headers[i].value, "close"))
            conn->flags |= WBY_CON_CLOSE_AFTER_RESPONSE;
        }
        wby_printf(conn_pub, "%s: %s\r\n", headers[i].name, headers[i].value);
    }

    if (!(conn->flags & WBY_CON_CLOSE_AFTER_RESPONSE)) {
        /* See if the client wants us to close the connection. */
        const char* connection_header = wby_find_header(conn_pub, "Connection");
        if (connection_header && !strcasecmp("close", connection_header)) {
            conn->flags |= WBY_CON_CLOSE_AFTER_RESPONSE;
            wby_printf(conn_pub, "Connection: close\r\n");
        }
    }
    wby_printf(conn_pub, "\r\n");
    if (content_length < 0)
        conn->flags |= WBY_CON_CHUNKED_RESPONSE;
    return 0;
}

WBY_API void
wby_response_end(struct wby_con *conn)
{
    struct wby_connection *conn_priv = (struct wby_connection*) conn;
    if (conn_priv->flags & WBY_CON_CHUNKED_RESPONSE) {
        /* Write final chunk */
        wby_connection_push(conn_priv, "0\r\n\r\n", 5);
        conn_priv->flags &= (wby_ushort)~WBY_CON_CHUNKED_RESPONSE;
    }
    /* Flush buffers */
    wby_connection_push(conn_priv, "", 0);
}

/* ===============================================================
 *                          SERVER
 * ===============================================================*/
/* Pointer to Integer type conversion for pointer alignment */
#if defined(__PTRDIFF_TYPE__) /* This case should work for GCC*/
# define WBY_UINT_TO_PTR(x) ((void*)(__PTRDIFF_TYPE__)(x))
# define WBY_PTR_TO_UINT(x) ((wby_size)(__PTRDIFF_TYPE__)(x))
#elif !defined(__GNUC__) /* works for compilers other than LLVM */
# define WBY_UINT_TO_PTR(x) ((void*)&((char*)0)[x])
# define WBY_PTR_TO_UINT(x) ((wby_size)(((char*)x)-(char*)0))
#elif defined(WBY_USE_FIXED_TYPES) /* used if we have <stdint.h> */
# define WBY_UINT_TO_PTR(x) ((void*)(uintptr_t)(x))
# define WBY_PTR_TO_UINT(x) ((uintptr_t)(x))
#else /* generates warning but works */
# define WBY_UINT_TO_PTR(x) ((void*)(x))
# define WBY_PTR_TO_UINT(x) ((wby_size)(x))
#endif

/* simple pointer math */
#define WBY_PTR_ADD(t, p, i) ((t*)((void*)((wby_byte*)(p) + (i))))
#define WBY_ALIGN_PTR(x, mask)\
    (WBY_UINT_TO_PTR((WBY_PTR_TO_UINT((wby_byte*)(x) + (mask-1)) & ~(mask-1))))

/* pointer alignment  */
#ifdef __cplusplus
template<typename T> struct wby_alignof;
template<typename T, int size_diff> struct wby_helper{enum {value = size_diff};};
template<typename T> struct wby_helper<T,0>{enum {value = wby_alignof<T>::value};};
template<typename T> struct wby_alignof{struct Big {T x; char c;}; enum {
    diff = sizeof(Big) - sizeof(T), value = wby_helper<Big, diff>::value};};
#define WBY_ALIGNOF(t) (wby_alignof<t>::value);
#else
#define WBY_ALIGNOF(t) ((char*)(&((struct {char c; t _h;}*)0)->_h) - (char*)0)
#endif

WBY_API void
wby_server_init(struct wby_server *srv, const struct wby_config *cfg, wby_size *needed_memory)
{
    WBY_STORAGE const wby_size wby_conn_align = WBY_ALIGNOF(struct wby_connection);
    WBY_ASSERT(srv);
    WBY_ASSERT(cfg);
    WBY_ASSERT(needed_memory);
    memset(srv, 0, sizeof(*srv));
    srv->config = *cfg;
    WBY_ASSERT(cfg->dispatch);

    *needed_memory = 0;
    *needed_memory += cfg->connection_max * sizeof(struct wby_connection);
    *needed_memory += cfg->connection_max * cfg->request_buffer_size;
    *needed_memory += cfg->connection_max * cfg->io_buffer_size;
    *needed_memory += wby_conn_align;
    srv->memory_size = *needed_memory;
}

WBY_API wby_int
wby_server_start(struct wby_server *server, void *memory)
{
    wby_size i;
    wby_socket sock;
    int on = 1;
    wby_byte *buffer = (wby_byte*)memory;
    struct sockaddr_in bind_addr;
    WBY_STORAGE const wby_size wby_conn_align = WBY_ALIGNOF(struct wby_connection);

    WBY_ASSERT(server);
    WBY_ASSERT(memory);
    memset(buffer, 0, server->memory_size);

    /* setup sever memory */
    server->socket = (wby_ptr)WBY_INVALID_SOCKET;
    server->con = (struct wby_connection*)WBY_ALIGN_PTR(buffer, wby_conn_align);
    buffer += ((wby_byte*)server->con - buffer);
    buffer += server->config.connection_max * sizeof(struct wby_connection);

    for (i = 0; i < server->config.connection_max; ++i) {
        server->con[i].log = server->config.log;
        server->con[i].header_buf.data = buffer;
        buffer += server->config.request_buffer_size;
        server->con[i].io_buf.data = buffer;
        server->con[i].request_buffer_size = server->config.request_buffer_size;
        server->con[i].io_buffer_size = server->config.io_buffer_size;
        buffer += server->config.io_buffer_size;
    }
    WBY_ASSERT((wby_size)(buffer - (wby_byte*)memory) <= server->memory_size);

    /* server socket setup */
    sock = (wby_ptr)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    wby_dbg(server->config.log, "Server socket = %d", (wby_int)sock);
    if (!wby_socket_is_valid(sock)) {
        wby_dbg(server->config.log, "failed to initialized server socket: %d", wby_socket_error());
        goto error;
    }
    setsockopt(WBY_SOCK(sock), SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    #ifdef __APPLE__ /* Don't generate SIGPIPE when writing to dead socket, we check all writes. */
    signal(SIGPIPE, SIG_IGN);
    #endif
    if (wby_socket_set_blocking(sock, 0) != WBY_OK) goto error;

    /* bind server socket */
    wby_dbg(server->config.log, "binding to %s:%d", server->config.address, server->config.port);
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons((wby_ushort)server->config.port);
    bind_addr.sin_addr.s_addr = inet_addr(server->config.address);
    if (bind(sock, (struct sockaddr*) &bind_addr, sizeof(bind_addr)) != WBY_OK) {
        wby_dbg(server->config.log, "bind() failed: %d", wby_socket_error());
        wby_dbg(server->config.log, "bind() failed: %s", strerror(wby_socket_error()));
        goto error;
    }

    /* set server socket to listening */
    if (listen(sock, SOMAXCONN) != WBY_OK) {
        wby_dbg(server->config.log, "listen() failed: %d", wby_socket_error());
        wby_socket_close(WBY_SOCK(sock));
        goto error;
    }
    server->socket = (wby_ptr)sock;
    wby_dbg(server->config.log, "server initialized: %s", strerror(errno));
    return 0;

error:
    if (wby_socket_is_valid(WBY_SOCK(sock)))
        wby_socket_close(WBY_SOCK(sock));
    return -1;
}

WBY_API void
wby_server_stop(struct wby_server *srv)
{
    wby_size i;
    wby_socket_close(WBY_SOCK(srv->socket));
    for (i = 0; i < srv->con_count; ++i)
        wby_socket_close(WBY_SOCK(srv->con[i].socket));
}

WBY_INTERN wby_int
wby_server_on_incoming(struct wby_server *srv)
{
    wby_size connection_index;
    char WBY_ALIGN(8) client_addr[64];
    struct wby_connection* connection;
    wby_socklen client_addr_len = sizeof(client_addr);
    wby_socket fd;

    /* Make sure we have space for a new connection */
    connection_index = srv->con_count;
    if (connection_index == srv->config.connection_max) {
        wby_dbg(srv->config.log, "out of connection slots");
        return 1;
    }

    /* Accept the incoming connection. */
    fd = accept(WBY_SOCK(srv->socket), (struct sockaddr*)&client_addr[0], &client_addr_len);
    if (!wby_socket_is_valid(fd)) {
        int err = wby_socket_error();
        if (!wby_socket_is_blocking_error(err))
            wby_dbg(srv->config.log, "accept() failed: %d", err);
        return 1;
    }

    connection = &srv->con[connection_index];
    wby_connection_reset(connection, srv->config.request_buffer_size, srv->config.io_buffer_size);
    connection->flags = WBY_CON_FRESH_CONNECTION;
    srv->con_count = connection_index + 1;

    /* Configure socket */
    if (wby_socket_config_incoming(fd) != WBY_OK) {
        wby_socket_close(fd);
        return 1;
    }

    /* OK, keep this connection */
    wby_dbg(srv->config.log, "tagging connection %d as alive", connection_index);
    connection->flags |= WBY_CON_ALIVE;
    connection->socket = (wby_ptr)fd;
    return 0;
}

WBY_INTERN void
wby_server_update_connection(struct wby_server *srv, struct wby_connection* connection)
{
    /* This is no longer a fresh connection. Only read from it when select() says
    * so in the future. */
    connection->flags &= (wby_ushort)~WBY_CON_FRESH_CONNECTION;
    for (;;)
    {
        switch (connection->state) {
        default: break;
        case WBY_REQUEST: {
            const char *expect_header;
            int request_size;
            int result = wby_socket_recv(WBY_SOCK(connection->socket),
                &connection->header_buf, srv->config.log);
            if (WBY_FILL_ERROR == result) {
                connection->flags &= (wby_ushort)~WBY_CON_ALIVE;
                return;
            }

            /* Scan to see if the buffer has a complete HTTP request header package. */
            request_size = wb_peek_request_size(connection->header_buf.data,
                (wby_int)connection->header_buf.used);
            if (request_size < 0) {
                /* Nothing yet. */
                if (connection->header_buf.max == connection->header_buf.used) {
                    wby_dbg(srv->config.log, "giving up as buffer is full");
                    /* Give up, we can't fit the request in our buffer. */
                    connection->flags &= (wby_ushort)~WBY_CON_ALIVE;
                }
                return;
            }
            wby_dbg(srv->config.log, "peek request size: %d", request_size);


            /* Set up request data. */
            if (wby_connection_setup_request(connection, request_size) != WBY_OK) {
                wby_dbg(srv->config.log, "failed to set up request");
                connection->flags &= (wby_ushort)~WBY_CON_ALIVE;
                return;
            }

            /* Remember how much of the remaining buffer is body data. */
            connection->header_body_left = (wby_int)connection->header_buf.used - request_size;
            /* If the client expects a 100 Continue, send one now. */
            if (NULL != (expect_header = wby_find_header(&connection->public_data, "Expect"))) {
                if (!strcasecmp(expect_header, "100-continue")) {
                    wby_dbg(srv->config.log, "connection expects a 100 Continue header.. making him happy");
                    connection->continue_data_left = (wby_int)wby_continue_header_len;
                    connection->state = WBY_SEND_CONTINUE;
                } else {
                    wby_dbg(srv->config.log, "unrecognized Expected header %s", expect_header);
                    connection->state = WBY_SERVE;
                }
            } else connection->state = WBY_SERVE;
        } break; /* WBY_REQUEST */

        case WBY_SEND_CONTINUE: {
            wby_int left = connection->continue_data_left;
            wby_int offset = (wby_int)wby_continue_header_len - left;
            wby_long written = 0;

            written = send(WBY_SOCK(connection->socket), wby_continue_header + offset, (wby_size)left, 0);
            wby_dbg(srv->config.log, "continue write: %d bytes", written);
            if (written < 0) {
                wby_dbg(srv->config.log, "failed to write 100-continue header");
                connection->flags &= (wby_ushort)~WBY_CON_ALIVE;
                return;
            }
            left -= (wby_int)written;
            connection->continue_data_left = left;
            if (left == 0)
                connection->state = WBY_SERVE;
        } break; /* WBY_SEND_cONTINUE */

        case WBY_SERVE: {
            /* Clear I/O buffer for output */
            connection->io_buf.used = 0;
            /* Switch socket to blocking mode. */
            if (wby_connection_set_blocking(connection) != WBY_OK)
                return;

            /* Figure out if this is a request to upgrade to WebSockets */
            if (wby_con_is_websocket_request(&connection->public_data)) {
                wby_dbg(srv->config.log, "received a websocket upgrade request");
                if (!srv->config.ws_connect ||
                    srv->config.ws_connect(&connection->public_data, srv->config.userdata) != WBY_OK)
                {
                    wby_dbg(srv->config.log, "user callback failed connection attempt");
                    wby_response_begin(&connection->public_data, 400, -1,
                        wby_plain_text_headers, WBY_LEN(wby_plain_text_headers));
                    wby_printf(&connection->public_data, "WebSockets not supported at %s\r\n",
                        connection->public_data.request.uri);
                    wby_response_end(&connection->public_data);
                } else {
                    /* OK, let's try to upgrade the connection to WebSockets */
                    if (wby_connection_send_websocket_upgrade(connection) != WBY_OK) {
                        wby_dbg(srv->config.log, "websocket upgrade failed");
                        wby_response_begin(&connection->public_data, 400, -1,
                            wby_plain_text_headers, WBY_LEN(wby_plain_text_headers));
                        wby_printf(&connection->public_data, "WebSockets couldn't not be enabled\r\n");
                        wby_response_end(&connection->public_data);
                    } else {
                        /* OK, we're now a websocket */
                        connection->flags |= WBY_CON_WEBSOCKET;
                        wby_dbg(srv->config.log, "connection %d upgraded to websocket",
                            (wby_int)(connection - srv->con));
                        srv->config.ws_connected(&connection->public_data, srv->config.userdata);
                    }
                }
            } else if (srv->config.dispatch(&connection->public_data, srv->config.userdata) != 0) {
                static const struct wby_header headers[] = {{ "Content-Type", "text/plain" }};
                wby_response_begin(&connection->public_data, 404, -1, headers, WBY_LEN(headers));
                wby_printf(&connection->public_data, "No handler for %s\r\n",
                    connection->public_data.request.uri);
                wby_response_end(&connection->public_data);
            }

            /* Back to non-blocking mode, can make the socket die. */
            wby_connection_set_nonblocking(connection);
            /* Ready for another request, unless we should close the connection. */
            if (connection->flags & WBY_CON_ALIVE) {
                if (connection->flags & WBY_CON_CLOSE_AFTER_RESPONSE) {
                    connection->flags &= (wby_ushort)~WBY_CON_ALIVE;
                    return;
                } else {
                    /* Reset connection for next request. */
                    wby_connection_reset(connection, srv->config.request_buffer_size,
                        srv->config.io_buffer_size);
                    if (!(connection->flags & WBY_CON_WEBSOCKET)) {
                        /* Loop back to request state */
                        connection->state = WBY_REQUEST;
                    } else {
                        /* Clear I/O buffer for input */
                        connection->io_buf.used = 0;
                        /* Go to the web socket serving state */
                        connection->state = WBY_WEBSOCKET;
                    }
                }
            }
        } break; /* WBY_SERVE */

        case WBY_WEBSOCKET: {
            /* In this state, we're trying to read a websocket frame into the I/O
            * buffer. Once we have enough data, we call the websocket frame
            * callback and let the client read the data through WebbyRead. */
            if (WBY_FILL_ERROR == wby_socket_recv(WBY_SOCK(connection->socket),
                &connection->io_buf, srv->config.log)) {
                /* Give up on this connection */
                connection->flags &= (wby_ushort)~WBY_CON_ALIVE;
                return;
            }

            if (wby_scan_websocket_frame(&connection->ws_frame, &connection->io_buf) != WBY_OK)
                return;

            connection->body_bytes_read = 0;
            connection->io_data_left = (wby_int)connection->io_buf.used - connection->ws_frame.header_size;
            wby_dbg(srv->config.log, "%d bytes of incoming websocket data buffered",
                (wby_int)connection->io_data_left);

            /* Switch socket to blocking mode */
            if (wby_connection_set_blocking(connection) != WBY_OK)
                return;

            switch (connection->ws_frame.opcode)
            {
            case WBY_WSOP_CLOSE:
                wby_dbg(srv->config.log, "received websocket close request");
                connection->flags &= (wby_ushort)~WBY_CON_ALIVE;
                return;

              case WBY_WSOP_PING:
                wby_dbg(srv->config.log, "received websocket ping request");
                if (wby_socket_send(WBY_SOCK(connection->socket), wby_websocket_pong,
                    sizeof wby_websocket_pong)){
                    connection->flags &= (wby_ushort)~WBY_CON_ALIVE;
                    return;
                }
                break;

              default:
                /* Dispatch frame to user handler. */
                if (srv->config.ws_frame(&connection->public_data,
                    &connection->ws_frame, srv->config.userdata) != WBY_OK) {
                  connection->flags &= (wby_ushort)~WBY_CON_ALIVE;
                  return;
                }
            }

            /* Discard any data the client didn't read to retain the socket state. */
            if (connection->body_bytes_read < connection->ws_frame.payload_length) {
                int size = connection->ws_frame.payload_length - connection->body_bytes_read;
                if (wby_con_discard_incoming_data(&connection->public_data, size) != WBY_OK) {
                    connection->flags &= (wby_ushort)~WBY_CON_ALIVE;
                    return;
                }
            }

            /* Back to non-blocking mode */
            if (wby_connection_set_nonblocking(connection) != WBY_OK)
                return;

            wby_connection_reset(connection, srv->config.request_buffer_size, srv->config.io_buffer_size);
            connection->state = WBY_WEBSOCKET;
        } break; /* WBY_WEBSOCKET */
        } /* switch */
    } /* for */
}

WBY_API void
wby_server_update(struct wby_server *srv)
{
    wby_int err;
    wby_size i, count;
    wby_socket max_socket;
    fd_set read_fds, write_fds, except_fds;
    struct timeval timeout;

    /* Build set of sockets to check for events */
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&except_fds);
    max_socket = 0;

    /* Only accept incoming connections if we have space */
    if (srv->con_count < srv->config.connection_max) {
        FD_SET(srv->socket, &read_fds);
        FD_SET(srv->socket, &except_fds);
        max_socket = WBY_SOCK(srv->socket);
    }

    for (i = 0, count = srv->con_count; i < count; ++i) {
        wby_socket socket = WBY_SOCK(srv->con[i].socket);
        FD_SET(socket, &read_fds);
        FD_SET(socket, &except_fds);
        if (srv->con[i].state == WBY_SEND_CONTINUE)
            FD_SET(socket, &write_fds);
        if (socket > max_socket)
            max_socket = socket;
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = 5;
    err = select((wby_int)(max_socket + 1), &read_fds, &write_fds, &except_fds, &timeout);
    if (err < 0) {
        wby_dbg(srv->config.log, "failed to select");
        return;
    }

    /* Handle incoming connections */
    if (FD_ISSET(WBY_SOCK(srv->socket), &read_fds)) {
        do {
            wby_dbg(srv->config.log, "awake on incoming");
            err = wby_server_on_incoming(srv);
        } while (err == 0);
    }

    /* Handle incoming connection data */
    for (i = 0, count = srv->con_count; i < count; ++i) {
        struct wby_connection *conn = &srv->con[i];
        if (FD_ISSET(WBY_SOCK(conn->socket), &read_fds) ||
            FD_ISSET(WBY_SOCK(conn->socket), &write_fds) ||
            conn->flags & WBY_CON_FRESH_CONNECTION)
        {
            wby_dbg(srv->config.log, "reading from connection %d", i);
            wby_server_update_connection(srv, conn);
        }
    }

    /* Close stale connections & compact connection array. */
    for (i = 0; i < srv->con_count; ) {
        struct wby_connection *connection = &srv->con[i];
        if (!(connection->flags & WBY_CON_ALIVE)) {
            wby_size remain;
            wby_dbg(srv->config.log, "closing connection %d (%08x)", i, connection->flags);
            if (connection->flags & WBY_CON_WEBSOCKET)
                srv->config.ws_closed(&connection->public_data, srv->config.userdata);
            remain = srv->con_count - (wby_size)i - 1;
            wby_connection_close(connection);
            memmove(&srv->con[i], &srv->con[i + 1], remain*sizeof(srv->con[i]));
            --srv->con_count;
        } else ++i;
    }
}

#endif /* WBY_IMPLEMENTATION */
