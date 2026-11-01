/*
 * weblite.c - Amalgamated implementation
 *
 * Embeddable C REST API client with HTTP/WebSocket transport.
 * This is a single-file implementation generated from the weblite source tree.
 *
 * Compile with: cc -c weblite.c -I.
 * Define WEBLITE_POSIX=1, WEBLITE_WIN32=1, WEBLITE_WASM=1, or WEBLITE_EMBEDDED=1
 * to select the platform. Define WEBLITE_HAS_TLS=1 to enable TLS (requires mbedtls).
 * Define WEBLITE_HAS_THREADS=1 to enable the async thread pool.
 */
#include "weblite.h"

/* System includes */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

/*****************************************************************************
 * Internal headers
 *****************************************************************************/

/* --- platform.h --- */


/*
 * Platform Abstraction Layer (PAL)
 *
 * Each platform_*.c implements these functions.
 * WASM platform implements higher-level hooks instead of raw sockets.
 */

/* Socket operations */
typedef int wl_socket_t;
#define WL_INVALID_SOCKET (-1)

int  wl_platform_init(void);
void wl_platform_cleanup(void);

/* TCP socket - returns WL_INVALID_SOCKET on error */
wl_socket_t wl_platform_socket_open(const char *host, uint16_t port, uint32_t timeout_ms);
void        wl_platform_socket_close(wl_socket_t sock);
int         wl_platform_socket_send(wl_socket_t sock, const void *data, size_t len);
int         wl_platform_socket_recv(wl_socket_t sock, void *buf, size_t len);

/* Set socket timeout (ms). 0 = no timeout */
int wl_platform_socket_set_timeout(wl_socket_t sock, uint32_t timeout_ms);

/* DNS resolution - returns 0 on success, fills addr_str (must be >= 46 bytes) */
int wl_platform_resolve(const char *host, char *addr_str, size_t addr_str_len);

/* Random bytes - returns 0 on success */
int wl_platform_random(void *buf, size_t len);

/* Time in milliseconds (monotonic) */
uint64_t wl_platform_time_ms(void);

/* Threading (only used when WEBLITE_HAS_THREADS) */
typedef void *wl_thread_t;
typedef void *wl_mutex_t;
typedef void *wl_cond_t;

typedef void *(*wl_thread_func_t)(void *arg);

wl_thread_t wl_platform_thread_create(wl_thread_func_t func, void *arg);
int         wl_platform_thread_join(wl_thread_t thread);

wl_mutex_t  wl_platform_mutex_create(void);
void        wl_platform_mutex_destroy(wl_mutex_t mutex);
void        wl_platform_mutex_lock(wl_mutex_t mutex);
void        wl_platform_mutex_unlock(wl_mutex_t mutex);

wl_cond_t   wl_platform_cond_create(void);
void        wl_platform_cond_destroy(wl_cond_t cond);
void        wl_platform_cond_wait(wl_cond_t cond, wl_mutex_t mutex);
int         wl_platform_cond_timedwait(wl_cond_t cond, wl_mutex_t mutex, uint32_t timeout_ms);
void        wl_platform_cond_signal(wl_cond_t cond);
void        wl_platform_cond_broadcast(wl_cond_t cond);


/* --- buf.h --- */


typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} wl_buf_t;

void   wl_buf_init(wl_buf_t *buf);
void   wl_buf_free(wl_buf_t *buf);
int    wl_buf_append(wl_buf_t *buf, const void *data, size_t len);
int    wl_buf_append_str(wl_buf_t *buf, const char *str);
int    wl_buf_append_char(wl_buf_t *buf, char c);
int    wl_buf_ensure(wl_buf_t *buf, size_t additional);
void   wl_buf_clear(wl_buf_t *buf);
char  *wl_buf_detach(wl_buf_t *buf, size_t *out_len);
int    wl_buf_append_fmt(wl_buf_t *buf, const char *fmt, ...);


/* --- url.h --- */


typedef struct {
    char    *scheme;    /* "http" or "https" or "ws" or "wss" */
    char    *host;
    uint16_t port;      /* 0 means use default for scheme */
    char    *path;      /* includes leading '/', empty path becomes "/" */
    char    *query;     /* after '?', may be NULL */
    char    *fragment;  /* after '#', may be NULL */
    int      is_tls;    /* 1 if https or wss */
} wl_url_t;

int  wl_url_parse(const char *url_str, wl_url_t *out);
void wl_url_free(wl_url_t *url);
uint16_t wl_url_default_port(const wl_url_t *url);


/* --- base64.h --- */


/* Returns allocated base64 string, caller must free. Returns NULL on error. */
char *wl_base64_encode(const void *data, size_t len);

/* Returns allocated decoded data, caller must free. *out_len set to decoded length. */
void *wl_base64_decode(const char *b64, size_t *out_len);

/* Returns required buffer size for encoding (including null terminator) */
size_t wl_base64_encode_len(size_t input_len);


/* --- sha1.h --- */


#define WL_SHA1_DIGEST_SIZE 20

typedef struct {
    uint32_t state[5];
    uint64_t count;
    uint8_t  buffer[64];
} wl_sha1_ctx_t;

void wl_sha1_init(wl_sha1_ctx_t *ctx);
void wl_sha1_update(wl_sha1_ctx_t *ctx, const void *data, size_t len);
void wl_sha1_final(wl_sha1_ctx_t *ctx, uint8_t digest[WL_SHA1_DIGEST_SIZE]);

/* Convenience: hash data in one call */
void wl_sha1(const void *data, size_t len, uint8_t digest[WL_SHA1_DIGEST_SIZE]);


/* --- wl_tls.h --- */


typedef struct wl_tls wl_tls_t;

/* Initialize TLS subsystem. Call once at startup. */
int  wl_tls_init(void);
void wl_tls_cleanup(void);

/* Create a TLS context wrapping an existing socket.
   hostname is used for SNI and certificate verification. */
wl_tls_t *wl_tls_connect(wl_socket_t sock, const char *hostname,
                          int verify, const char *ca_path);

/* Send/recv through TLS. Same semantics as platform socket_send/recv. */
int  wl_tls_send(wl_tls_t *tls, const void *data, size_t len);
int  wl_tls_recv(wl_tls_t *tls, void *buf, size_t len);

/* Close TLS session (does NOT close the underlying socket) */
void wl_tls_close(wl_tls_t *tls);


/* --- http.h --- */


/* Build an HTTP/1.1 request into buf.
   path should include query string if any. */
int wl_http_build_request(wl_buf_t *buf,
                          weblite_method_t method,
                          const char *host,
                          const char *path,
                          const weblite_header_t *headers,
                          size_t header_count,
                          const void *body,
                          size_t body_len);

/* Parse an HTTP/1.1 response from raw data.
   Returns 0 on success, -1 on parse error, 1 if more data needed.
   On success, populates resp and sets *body_offset to where body starts. */
int wl_http_parse_response(const char *data, size_t data_len,
                           weblite_response_t *resp,
                           size_t *body_offset);

/* Get method string */
const char *wl_http_method_str(weblite_method_t method);

/* Parse Content-Length from headers. Returns -1 if not found. */
int64_t wl_http_get_content_length(const weblite_header_owned_t *headers,
                                    size_t header_count);

/* Check if response uses chunked transfer encoding */
int wl_http_is_chunked(const weblite_header_owned_t *headers, size_t header_count);

/* Decode chunked body. Returns decoded data (caller frees), sets *out_len. */
void *wl_http_decode_chunked(const char *data, size_t data_len, size_t *out_len);

/* Get Location header for redirects. Returns NULL if not found. */
const char *wl_http_get_location(const weblite_header_owned_t *headers,
                                  size_t header_count);


/* --- ws.h --- */


/* WebSocket opcode types */
typedef enum {
    WL_WS_OP_CONTINUATION = 0x0,
    WL_WS_OP_TEXT         = 0x1,
    WL_WS_OP_BINARY       = 0x2,
    WL_WS_OP_CLOSE        = 0x8,
    WL_WS_OP_PING         = 0x9,
    WL_WS_OP_PONG         = 0xA
} wl_ws_opcode_t;

/* WebSocket connection */
typedef struct {
    wl_socket_t  sock;
#ifdef WEBLITE_HAS_TLS
    wl_tls_t    *tls;
#endif
    int          is_tls;
#ifdef WEBLITE_HAS_COMPRESSION
    int          compress_enabled;         /* 1 if permessage-deflate negotiated */
    int          server_no_context_takeover;
    int          client_no_context_takeover;
    int          server_max_window_bits;   /* 8-15, default 15 */
    int          client_max_window_bits;   /* 8-15, default 15 */
    void        *inflate_ctx;              /* z_stream* for decompression */
    void        *deflate_ctx;              /* z_stream* for compression */
#endif
} wl_ws_conn_t;

/* Perform WebSocket handshake over an existing connection.
   Returns 0 on success, -1 on failure. */
int wl_ws_handshake(wl_ws_conn_t *conn, const char *host, const char *path);

/* Send a WebSocket frame. Data is masked per RFC 6455 (client requirement). */
int wl_ws_send_frame(wl_ws_conn_t *conn, wl_ws_opcode_t opcode,
                     const void *data, size_t len);

/* Receive a WebSocket frame. Handles control frames (ping/pong/close) internally.
   Returns data frame payload in *out_data (caller frees), *out_len.
   Returns opcode in *out_opcode. Returns 0 on success. */
int wl_ws_recv_frame(wl_ws_conn_t *conn, wl_ws_opcode_t *out_opcode,
                     void **out_data, size_t *out_len);

/* Send close frame and shutdown */
void wl_ws_close(wl_ws_conn_t *conn);

/* Tunnel a REST request over WebSocket.
   Encodes request as JSON, sends as text frame, receives JSON response.
   Returns 0 on success. */
int wl_ws_rest_request(wl_ws_conn_t *conn,
                       weblite_method_t method,
                       const char *path,
                       const weblite_header_t *headers,
                       size_t header_count,
                       const void *body,
                       size_t body_len,
                       weblite_response_t *resp);

/* Clean up compression state (call before freeing conn) */
void wl_ws_compression_cleanup(wl_ws_conn_t *conn);

/* Low-level send/recv that use TLS if available */
int wl_ws_raw_send(wl_ws_conn_t *conn, const void *data, size_t len);
int wl_ws_raw_recv(wl_ws_conn_t *conn, void *buf, size_t len);


/* --- transport.h --- */


/* Execute a request using the configured transport.
   Handles transport selection, connection, TLS, and HTTP->WS fallback. */
weblite_error_t wl_transport_execute(const weblite_request_t *req,
                                     weblite_response_t *resp,
                                     weblite_transport_mode_t mode,
                                     uint32_t timeout_ms,
                                     uint32_t max_redirects,
                                     int tls_verify,
                                     const char *ca_path);


/* --- async.h --- */


/* Initialize the async subsystem with the given thread pool size */
int  wl_async_init(int thread_count);
void wl_async_cleanup(void);

/* Submit an async request. Returns a handle. */
weblite_async_handle_t *wl_async_submit(const weblite_request_t *req,
                                         weblite_callback_t callback,
                                         void *user_data,
                                         const weblite_config_t *config);

/* Cancel an async request */
weblite_error_t wl_async_cancel(weblite_async_handle_t *handle);

/* Wait for an async request to complete */
weblite_error_t wl_async_wait(weblite_async_handle_t *handle, uint32_t timeout_ms);

/* Poll for completed async requests (for no-thread platforms) */
int wl_async_poll(void);


/*****************************************************************************
 * Implementation
 *****************************************************************************/

/* --- buf.c --- */

#define WL_BUF_INIT_CAP 256

void wl_buf_init(wl_buf_t *buf) {
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

void wl_buf_free(wl_buf_t *buf) {
    free(buf->data);
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

int wl_buf_ensure(wl_buf_t *buf, size_t additional) {
    size_t needed = buf->len + additional;
    if (needed <= buf->cap)
        return 0;

    size_t new_cap = buf->cap ? buf->cap : WL_BUF_INIT_CAP;
    while (new_cap < needed)
        new_cap *= 2;

    char *new_data = realloc(buf->data, new_cap);
    if (!new_data)
        return -1;

    buf->data = new_data;
    buf->cap = new_cap;
    return 0;
}

int wl_buf_append(wl_buf_t *buf, const void *data, size_t len) {
    if (len == 0)
        return 0;
    if (wl_buf_ensure(buf, len) != 0)
        return -1;
    memcpy(buf->data + buf->len, data, len);
    buf->len += len;
    return 0;
}

int wl_buf_append_str(wl_buf_t *buf, const char *str) {
    return wl_buf_append(buf, str, strlen(str));
}

int wl_buf_append_char(wl_buf_t *buf, char c) {
    return wl_buf_append(buf, &c, 1);
}

void wl_buf_clear(wl_buf_t *buf) {
    buf->len = 0;
}

char *wl_buf_detach(wl_buf_t *buf, size_t *out_len) {
    /* Null-terminate for convenience */
    if (wl_buf_append_char(buf, '\0') != 0)
        return NULL;
    buf->len--; /* don't count the null in len */

    char *data = buf->data;
    if (out_len)
        *out_len = buf->len;

    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
    return data;
}

int wl_buf_append_fmt(wl_buf_t *buf, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);

    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (needed < 0) {
        va_end(ap2);
        return -1;
    }

    if (wl_buf_ensure(buf, (size_t)needed + 1) != 0) {
        va_end(ap2);
        return -1;
    }

    vsnprintf(buf->data + buf->len, (size_t)needed + 1, fmt, ap2);
    va_end(ap2);
    buf->len += (size_t)needed;
    return 0;
}

/* --- url.c --- */

static char *wl_strdup(const char *s) {
    size_t len = strlen(s);
    char *dup = malloc(len + 1);
    if (dup) memcpy(dup, s, len + 1);
    return dup;
}

static char *wl_strndup(const char *s, size_t n) {
    char *dup = malloc(n + 1);
    if (dup) {
        memcpy(dup, s, n);
        dup[n] = '\0';
    }
    return dup;
}

int wl_url_parse(const char *url_str, wl_url_t *out) {
    if (!url_str || !out)
        return -1;

    memset(out, 0, sizeof(*out));

    const char *p = url_str;

    /* Parse scheme */
    const char *scheme_end = strstr(p, "://");
    if (!scheme_end)
        return -1;

    out->scheme = wl_strndup(p, (size_t)(scheme_end - p));
    if (!out->scheme)
        goto fail;

    /* Lowercase the scheme */
    for (char *s = out->scheme; *s; s++)
        *s = (char)tolower((unsigned char)*s);

    out->is_tls = (strcmp(out->scheme, "https") == 0 ||
                   strcmp(out->scheme, "wss") == 0);

    p = scheme_end + 3;

    /* Parse host (and optional port) */
    const char *host_start = p;
    const char *host_end = NULL;
    const char *port_str = NULL;

    /* Find end of authority (host:port) */
    const char *slash = strchr(p, '/');
    const char *qmark = strchr(p, '?');
    const char *hash = strchr(p, '#');

    /* authority_end is the earliest of /, ?, #, or end of string */
    const char *authority_end = p + strlen(p);
    if (slash && slash < authority_end) authority_end = slash;
    if (qmark && qmark < authority_end) authority_end = qmark;
    if (hash && hash < authority_end) authority_end = hash;

    /* Check for port */
    const char *colon = NULL;
    /* Scan backwards from authority_end for ':' */
    for (const char *c = authority_end - 1; c >= host_start; c--) {
        if (*c == ':') {
            colon = c;
            break;
        }
    }

    if (colon && colon > host_start) {
        /* Verify everything after colon is digits */
        int all_digits = 1;
        for (const char *d = colon + 1; d < authority_end; d++) {
            if (!isdigit((unsigned char)*d)) {
                all_digits = 0;
                break;
            }
        }
        if (all_digits && colon + 1 < authority_end) {
            host_end = colon;
            port_str = colon + 1;
        }
    }

    if (!host_end)
        host_end = authority_end;

    if (host_end == host_start)
        goto fail; /* empty host */

    out->host = wl_strndup(host_start, (size_t)(host_end - host_start));
    if (!out->host)
        goto fail;

    if (port_str) {
        long port = strtol(port_str, NULL, 10);
        if (port <= 0 || port > 65535)
            goto fail;
        out->port = (uint16_t)port;
    }

    p = authority_end;

    /* Parse path */
    if (*p == '/') {
        const char *path_end = p;
        while (*path_end && *path_end != '?' && *path_end != '#')
            path_end++;
        out->path = wl_strndup(p, (size_t)(path_end - p));
        p = path_end;
    } else {
        out->path = wl_strdup("/");
    }
    if (!out->path)
        goto fail;

    /* Parse query */
    if (*p == '?') {
        p++;
        const char *query_end = p;
        while (*query_end && *query_end != '#')
            query_end++;
        out->query = wl_strndup(p, (size_t)(query_end - p));
        if (!out->query)
            goto fail;
        p = query_end;
    }

    /* Parse fragment */
    if (*p == '#') {
        p++;
        out->fragment = wl_strdup(p);
        if (!out->fragment)
            goto fail;
    }

    return 0;

fail:
    wl_url_free(out);
    return -1;
}

void wl_url_free(wl_url_t *url) {
    if (!url) return;
    free(url->scheme);
    free(url->host);
    free(url->path);
    free(url->query);
    free(url->fragment);
    memset(url, 0, sizeof(*url));
}

uint16_t wl_url_default_port(const wl_url_t *url) {
    if (url->port != 0)
        return url->port;
    if (strcmp(url->scheme, "https") == 0 || strcmp(url->scheme, "wss") == 0)
        return 443;
    return 80;
}

/* --- base64.c --- */

static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const unsigned char b64_decode_table[256] = {
    ['A'] = 0,  ['B'] = 1,  ['C'] = 2,  ['D'] = 3,
    ['E'] = 4,  ['F'] = 5,  ['G'] = 6,  ['H'] = 7,
    ['I'] = 8,  ['J'] = 9,  ['K'] = 10, ['L'] = 11,
    ['M'] = 12, ['N'] = 13, ['O'] = 14, ['P'] = 15,
    ['Q'] = 16, ['R'] = 17, ['S'] = 18, ['T'] = 19,
    ['U'] = 20, ['V'] = 21, ['W'] = 22, ['X'] = 23,
    ['Y'] = 24, ['Z'] = 25,
    ['a'] = 26, ['b'] = 27, ['c'] = 28, ['d'] = 29,
    ['e'] = 30, ['f'] = 31, ['g'] = 32, ['h'] = 33,
    ['i'] = 34, ['j'] = 35, ['k'] = 36, ['l'] = 37,
    ['m'] = 38, ['n'] = 39, ['o'] = 40, ['p'] = 41,
    ['q'] = 42, ['r'] = 43, ['s'] = 44, ['t'] = 45,
    ['u'] = 46, ['v'] = 47, ['w'] = 48, ['x'] = 49,
    ['y'] = 50, ['z'] = 51,
    ['0'] = 52, ['1'] = 53, ['2'] = 54, ['3'] = 55,
    ['4'] = 56, ['5'] = 57, ['6'] = 58, ['7'] = 59,
    ['8'] = 60, ['9'] = 61, ['+'] = 62, ['/'] = 63,
};

size_t wl_base64_encode_len(size_t input_len) {
    return ((input_len + 2) / 3) * 4 + 1;
}

char *wl_base64_encode(const void *data, size_t len) {
    const unsigned char *in = data;
    size_t out_len = wl_base64_encode_len(len);
    char *out = malloc(out_len);
    if (!out) return NULL;

    size_t i = 0, j = 0;
    while (i + 2 < len) {
        uint32_t v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i+1] << 8) | in[i+2];
        out[j++] = b64_table[(v >> 18) & 0x3F];
        out[j++] = b64_table[(v >> 12) & 0x3F];
        out[j++] = b64_table[(v >> 6) & 0x3F];
        out[j++] = b64_table[v & 0x3F];
        i += 3;
    }

    if (i < len) {
        uint32_t v = (uint32_t)in[i] << 16;
        if (i + 1 < len)
            v |= (uint32_t)in[i+1] << 8;

        out[j++] = b64_table[(v >> 18) & 0x3F];
        out[j++] = b64_table[(v >> 12) & 0x3F];
        out[j++] = (i + 1 < len) ? b64_table[(v >> 6) & 0x3F] : '=';
        out[j++] = '=';
    }

    out[j] = '\0';
    return out;
}

void *wl_base64_decode(const char *b64, size_t *out_len) {
    if (!b64) return NULL;

    size_t input_len = strlen(b64);
    if (input_len == 0) {
        char *empty = malloc(1);
        if (empty) empty[0] = '\0';
        if (out_len) *out_len = 0;
        return empty;
    }

    if (input_len % 4 != 0)
        return NULL;

    size_t decoded_len = (input_len / 4) * 3;
    if (b64[input_len - 1] == '=') decoded_len--;
    if (b64[input_len - 2] == '=') decoded_len--;

    unsigned char *out = malloc(decoded_len + 1);
    if (!out) return NULL;

    size_t i = 0, j = 0;
    while (i < input_len) {
        uint32_t a = b64_decode_table[(unsigned char)b64[i++]];
        uint32_t b = b64_decode_table[(unsigned char)b64[i++]];
        uint32_t c = b64_decode_table[(unsigned char)b64[i++]];
        uint32_t d = b64_decode_table[(unsigned char)b64[i++]];

        uint32_t v = (a << 18) | (b << 12) | (c << 6) | d;

        if (j < decoded_len) out[j++] = (unsigned char)(v >> 16);
        if (j < decoded_len) out[j++] = (unsigned char)(v >> 8);
        if (j < decoded_len) out[j++] = (unsigned char)(v);
    }

    out[decoded_len] = '\0';
    if (out_len) *out_len = decoded_len;
    return out;
}

/* --- sha1.c --- */

static uint32_t rol32(uint32_t v, unsigned n) {
    return (v << n) | (v >> (32 - n));
}

static void sha1_transform(uint32_t state[5], const uint8_t block[64]) {
    uint32_t w[80];
    unsigned i;

    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i*4] << 24)
             | ((uint32_t)block[i*4+1] << 16)
             | ((uint32_t)block[i*4+2] << 8)
             | ((uint32_t)block[i*4+3]);
    }
    for (i = 16; i < 80; i++)
        w[i] = rol32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);

    uint32_t a = state[0], b = state[1], c = state[2];
    uint32_t d = state[3], e = state[4];

    for (i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        uint32_t temp = rol32(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rol32(b, 30);
        b = a;
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

void wl_sha1_init(wl_sha1_ctx_t *ctx) {
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count = 0;
    memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

void wl_sha1_update(wl_sha1_ctx_t *ctx, const void *data, size_t len) {
    const uint8_t *p = data;
    size_t buf_idx = (size_t)(ctx->count % 64);
    ctx->count += len;

    if (buf_idx > 0) {
        size_t space = 64 - buf_idx;
        size_t copy = len < space ? len : space;
        memcpy(ctx->buffer + buf_idx, p, copy);
        p += copy;
        len -= copy;
        buf_idx += copy;
        if (buf_idx == 64) {
            sha1_transform(ctx->state, ctx->buffer);
            buf_idx = 0;
        }
    }

    while (len >= 64) {
        sha1_transform(ctx->state, p);
        p += 64;
        len -= 64;
    }

    if (len > 0)
        memcpy(ctx->buffer, p, len);
}

void wl_sha1_final(wl_sha1_ctx_t *ctx, uint8_t digest[WL_SHA1_DIGEST_SIZE]) {
    uint64_t total_bits = ctx->count * 8;
    size_t buf_idx = (size_t)(ctx->count % 64);

    ctx->buffer[buf_idx++] = 0x80;

    if (buf_idx > 56) {
        memset(ctx->buffer + buf_idx, 0, 64 - buf_idx);
        sha1_transform(ctx->state, ctx->buffer);
        buf_idx = 0;
    }

    memset(ctx->buffer + buf_idx, 0, 56 - buf_idx);

    /* Append length in big-endian */
    ctx->buffer[56] = (uint8_t)(total_bits >> 56);
    ctx->buffer[57] = (uint8_t)(total_bits >> 48);
    ctx->buffer[58] = (uint8_t)(total_bits >> 40);
    ctx->buffer[59] = (uint8_t)(total_bits >> 32);
    ctx->buffer[60] = (uint8_t)(total_bits >> 24);
    ctx->buffer[61] = (uint8_t)(total_bits >> 16);
    ctx->buffer[62] = (uint8_t)(total_bits >> 8);
    ctx->buffer[63] = (uint8_t)(total_bits);

    sha1_transform(ctx->state, ctx->buffer);

    for (int i = 0; i < 5; i++) {
        digest[i*4]   = (uint8_t)(ctx->state[i] >> 24);
        digest[i*4+1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i*4+2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i*4+3] = (uint8_t)(ctx->state[i]);
    }
}

void wl_sha1(const void *data, size_t len, uint8_t digest[WL_SHA1_DIGEST_SIZE]) {
    wl_sha1_ctx_t ctx;
    wl_sha1_init(&ctx);
    wl_sha1_update(&ctx, data, len);
    wl_sha1_final(&ctx, digest);
}

/* --- tls.c --- */

/*
 * Stub implementations used when no TLS backend is selected
 * (WEBLITE_TLS_BACKEND=none). When a backend IS selected, the real
 * implementations live in tls_mbedtls.c, tls_openssl.c, or tls_libressl.c
 * and these stubs are overridden at link time.
 */

#ifndef WEBLITE_HAS_TLS

int  wl_tls_init(void) { return 0; }
void wl_tls_cleanup(void) { }

wl_tls_t *wl_tls_connect(wl_socket_t sock, const char *hostname,
                          int verify, const char *ca_path) {
    (void)sock; (void)hostname; (void)verify; (void)ca_path;
    return NULL;
}

int wl_tls_send(wl_tls_t *tls, const void *data, size_t len) {
    (void)tls; (void)data; (void)len;
    return -1;
}

int wl_tls_recv(wl_tls_t *tls, void *buf, size_t len) {
    (void)tls; (void)buf; (void)len;
    return -1;
}

void wl_tls_close(wl_tls_t *tls) {
    (void)tls;
}

#endif /* !WEBLITE_HAS_TLS */

/* --- http.c --- */

const char *wl_http_method_str(weblite_method_t method) {
    switch (method) {
        case WEBLITE_METHOD_GET:     return "GET";
        case WEBLITE_METHOD_POST:    return "POST";
        case WEBLITE_METHOD_PUT:     return "PUT";
        case WEBLITE_METHOD_DELETE:  return "DELETE";
        case WEBLITE_METHOD_PATCH:   return "PATCH";
        case WEBLITE_METHOD_HEAD:    return "HEAD";
        case WEBLITE_METHOD_OPTIONS: return "OPTIONS";
    }
    return "GET";
}

int wl_http_build_request(wl_buf_t *buf,
                          weblite_method_t method,
                          const char *host,
                          const char *path,
                          const weblite_header_t *headers,
                          size_t header_count,
                          const void *body,
                          size_t body_len) {
    /* Request line */
    if (wl_buf_append_fmt(buf, "%s %s HTTP/1.1\r\n", wl_http_method_str(method), path) != 0)
        return -1;

    /* Host header */
    if (wl_buf_append_fmt(buf, "Host: %s\r\n", host) != 0)
        return -1;

    /* User headers */
    int has_content_length = 0;
    int has_connection = 0;
    for (size_t i = 0; i < header_count; i++) {
        if (wl_buf_append_fmt(buf, "%s: %s\r\n", headers[i].name, headers[i].value) != 0)
            return -1;
        if (strcasecmp(headers[i].name, "Content-Length") == 0)
            has_content_length = 1;
        if (strcasecmp(headers[i].name, "Connection") == 0)
            has_connection = 1;
    }

    /* Auto Content-Length for body */
    if (body_len > 0 && !has_content_length) {
        if (wl_buf_append_fmt(buf, "Content-Length: %zu\r\n", body_len) != 0)
            return -1;
    }

    /* Default Connection: close */
    if (!has_connection) {
        if (wl_buf_append_str(buf, "Connection: close\r\n") != 0)
            return -1;
    }

    /* End of headers */
    if (wl_buf_append_str(buf, "\r\n") != 0)
        return -1;

    /* Body */
    if (body && body_len > 0) {
        if (wl_buf_append(buf, body, body_len) != 0)
            return -1;
    }

    return 0;
}

/* Find \r\n\r\n in data */
static const char *find_header_end(const char *data, size_t len) {
    for (size_t i = 0; i + 3 < len; i++) {
        if (data[i] == '\r' && data[i+1] == '\n' &&
            data[i+2] == '\r' && data[i+3] == '\n')
            return data + i;
    }
    return NULL;
}

static char *strndup_local(const char *s, size_t n) {
    char *d = malloc(n + 1);
    if (d) { memcpy(d, s, n); d[n] = '\0'; }
    return d;
}

int wl_http_parse_response(const char *data, size_t data_len,
                           weblite_response_t *resp,
                           size_t *body_offset) {
    const char *header_end = find_header_end(data, data_len);
    if (!header_end)
        return 1; /* need more data */

    *body_offset = (size_t)(header_end - data) + 4;

    /* Parse status line: "HTTP/1.1 200 OK\r\n" */
    const char *p = data;
    const char *line_end = strstr(p, "\r\n");
    if (!line_end)
        return -1;

    /* Skip "HTTP/x.x " */
    const char *space = memchr(p, ' ', (size_t)(line_end - p));
    if (!space)
        return -1;
    space++;

    resp->status_code = (int)strtol(space, NULL, 10);
    if (resp->status_code < 100 || resp->status_code > 599)
        return -1;

    /* Parse headers */
    p = line_end + 2;
    size_t max_headers = 64;
    resp->headers = calloc(max_headers, sizeof(weblite_header_owned_t));
    if (!resp->headers)
        return -1;
    resp->header_count = 0;

    while (p < header_end) {
        line_end = strstr(p, "\r\n");
        if (!line_end || line_end == p)
            break;

        const char *colon = memchr(p, ':', (size_t)(line_end - p));
        if (!colon) {
            p = line_end + 2;
            continue;
        }

        if (resp->header_count >= max_headers) {
            max_headers *= 2;
            weblite_header_owned_t *new_hdrs = realloc(resp->headers,
                max_headers * sizeof(weblite_header_owned_t));
            if (!new_hdrs)
                return -1;
            resp->headers = new_hdrs;
        }

        size_t name_len = (size_t)(colon - p);
        const char *val = colon + 1;
        while (val < line_end && (*val == ' ' || *val == '\t'))
            val++;
        size_t val_len = (size_t)(line_end - val);

        resp->headers[resp->header_count].name = strndup_local(p, name_len);
        resp->headers[resp->header_count].value = strndup_local(val, val_len);
        resp->header_count++;

        p = line_end + 2;
    }

    return 0;
}

int64_t wl_http_get_content_length(const weblite_header_owned_t *headers,
                                    size_t header_count) {
    for (size_t i = 0; i < header_count; i++) {
        if (strcasecmp(headers[i].name, "Content-Length") == 0)
            return strtoll(headers[i].value, NULL, 10);
    }
    return -1;
}

int wl_http_is_chunked(const weblite_header_owned_t *headers, size_t header_count) {
    for (size_t i = 0; i < header_count; i++) {
        if (strcasecmp(headers[i].name, "Transfer-Encoding") == 0 &&
            strcasestr(headers[i].value, "chunked") != NULL)
            return 1;
    }
    return 0;
}

void *wl_http_decode_chunked(const char *data, size_t data_len, size_t *out_len) {
    wl_buf_t buf;
    wl_buf_init(&buf);

    const char *p = data;
    const char *end = data + data_len;

    while (p < end) {
        /* Parse chunk size (hex) */
        char *endptr;
        unsigned long chunk_size = strtoul(p, &endptr, 16);
        if (endptr == p)
            break;

        /* Skip to data after \r\n */
        const char *crlf = strstr(endptr, "\r\n");
        if (!crlf)
            break;
        p = crlf + 2;

        if (chunk_size == 0)
            break; /* Final chunk */

        if (p + chunk_size > end)
            break;

        wl_buf_append(&buf, p, chunk_size);
        p += chunk_size;

        /* Skip trailing \r\n */
        if (p + 2 <= end && p[0] == '\r' && p[1] == '\n')
            p += 2;
    }

    *out_len = buf.len;
    return wl_buf_detach(&buf, NULL);
}

const char *wl_http_get_location(const weblite_header_owned_t *headers,
                                  size_t header_count) {
    for (size_t i = 0; i < header_count; i++) {
        if (strcasecmp(headers[i].name, "Location") == 0)
            return headers[i].value;
    }
    return NULL;
}

/* --- ws.c --- */

#ifdef WEBLITE_HAS_COMPRESSION
#include <zlib.h>
#endif

/* RFC 6455 WebSocket GUID */
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

int wl_ws_raw_send(wl_ws_conn_t *conn, const void *data, size_t len) {
#ifdef WEBLITE_HAS_TLS
    if (conn->is_tls && conn->tls)
        return wl_tls_send(conn->tls, data, len);
#endif
    return wl_platform_socket_send(conn->sock, data, len);
}

int wl_ws_raw_recv(wl_ws_conn_t *conn, void *buf, size_t len) {
#ifdef WEBLITE_HAS_TLS
    if (conn->is_tls && conn->tls)
        return wl_tls_recv(conn->tls, buf, len);
#endif
    return wl_platform_socket_recv(conn->sock, buf, len);
}

/* Read exactly n bytes */
static int recv_exact(wl_ws_conn_t *conn, void *buf, size_t n) {
    size_t got = 0;
    while (got < n) {
        int r = wl_ws_raw_recv(conn, (char *)buf + got, n - got);
        if (r <= 0) return -1;
        got += (size_t)r;
    }
    return 0;
}

/* Send a close frame containing only a 2-byte status code */
static void send_close_code(wl_ws_conn_t *conn, uint16_t code) {
    uint8_t payload[2] = { (uint8_t)(code >> 8), (uint8_t)(code & 0xFF) };
    wl_ws_send_frame(conn, WL_WS_OP_CLOSE, payload, 2);
}

/* ---- permessage-deflate compression (RFC 7692) ---- */

#ifdef WEBLITE_HAS_COMPRESSION

static int ws_deflate_init(wl_ws_conn_t *conn) {
    z_stream *strm = calloc(1, sizeof(z_stream));
    if (!strm) return -1;
    int wbits = conn->client_max_window_bits ? conn->client_max_window_bits : 15;
    if (deflateInit2(strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                     -wbits, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        free(strm);
        return -1;
    }
    conn->deflate_ctx = strm;
    return 0;
}

static int ws_inflate_init(wl_ws_conn_t *conn) {
    z_stream *strm = calloc(1, sizeof(z_stream));
    if (!strm) return -1;
    int wbits = conn->server_max_window_bits ? conn->server_max_window_bits : 15;
    if (inflateInit2(strm, -wbits) != Z_OK) {
        free(strm);
        return -1;
    }
    conn->inflate_ctx = strm;
    return 0;
}

void wl_ws_compression_cleanup(wl_ws_conn_t *conn) {
    if (!conn) return;
    if (conn->deflate_ctx) {
        deflateEnd((z_stream *)conn->deflate_ctx);
        free(conn->deflate_ctx);
        conn->deflate_ctx = NULL;
    }
    if (conn->inflate_ctx) {
        inflateEnd((z_stream *)conn->inflate_ctx);
        free(conn->inflate_ctx);
        conn->inflate_ctx = NULL;
    }
    conn->compress_enabled = 0;
}

/* Compress data with DEFLATE, stripping the trailing 00 00 FF FF flush marker.
   Caller must free *out. Returns 0 on success. */
static int ws_compress(wl_ws_conn_t *conn, const void *in, size_t in_len,
                       void **out, size_t *out_len) {
    z_stream *strm = (z_stream *)conn->deflate_ctx;
    /* deflateBound is only guaranteed for Z_FINISH/Z_NO_FLUSH, not Z_SYNC_FLUSH.
       Add extra space for the sync flush marker (empty stored block = 5 bytes). */
    size_t cap = deflateBound(strm, (uLong)in_len) + 16;
    uint8_t *buf = malloc(cap);
    if (!buf) return -1;

    strm->next_in = (Bytef *)(uintptr_t)in;
    strm->avail_in = (uInt)in_len;

    size_t total = 0;
    int rc;
    do {
        if (total == cap) {
            cap *= 2;
            uint8_t *tmp = realloc(buf, cap);
            if (!tmp) { free(buf); return -1; }
            buf = tmp;
        }
        strm->next_out = buf + total;
        strm->avail_out = (uInt)(cap - total);
        rc = deflate(strm, Z_SYNC_FLUSH);
        if (rc != Z_OK && rc != Z_BUF_ERROR) {
            free(buf);
            return -1;
        }
        total = cap - strm->avail_out;
    } while (strm->avail_in > 0 || strm->avail_out == 0);

    /* Z_SYNC_FLUSH always ends with 00 00 FF FF — strip it (RFC 7692 §7.2.1) */
    if (total >= 4 &&
        buf[total - 4] == 0x00 && buf[total - 3] == 0x00 &&
        buf[total - 2] == 0xFF && buf[total - 1] == 0xFF) {
        total -= 4;
    }

    if (conn->client_no_context_takeover) {
        deflateReset(strm);
    }

    *out = buf;
    *out_len = total;
    return 0;
}

/* Decompress data by appending the 00 00 FF FF trailer and inflating.
   Result is appended to out_buf. Returns 0 on success. */
static int ws_decompress(wl_ws_conn_t *conn, const void *in, size_t in_len,
                         wl_buf_t *out_buf) {
    z_stream *strm = (z_stream *)conn->inflate_ctx;
    /* Build input: compressed data + flush trailer */
    static const uint8_t trailer[] = { 0x00, 0x00, 0xFF, 0xFF };
    size_t total_in = in_len + 4;
    uint8_t *input = malloc(total_in);
    if (!input) return -1;
    memcpy(input, in, in_len);
    memcpy(input + in_len, trailer, 4);

    strm->next_in = input;
    strm->avail_in = (uInt)total_in;

    uint8_t chunk[16384];
    int rc;
    do {
        strm->next_out = chunk;
        strm->avail_out = sizeof(chunk);
        rc = inflate(strm, Z_SYNC_FLUSH);
        if (rc != Z_OK && rc != Z_STREAM_END && rc != Z_BUF_ERROR) {
            free(input);
            return -1;
        }
        size_t have = sizeof(chunk) - strm->avail_out;
        if (have > 0)
            wl_buf_append(out_buf, chunk, have);
    } while (strm->avail_in > 0 || strm->avail_out == 0);

    free(input);

    if (conn->server_no_context_takeover) {
        inflateReset(strm);
    }

    return 0;
}

#else /* !WEBLITE_HAS_COMPRESSION */

void wl_ws_compression_cleanup(wl_ws_conn_t *conn) {
    (void)conn;
}

#endif /* WEBLITE_HAS_COMPRESSION */

/*
 * Incremental UTF-8 validator (Bjoern Hoehrmann's DFA approach).
 * - state tracks position across calls (start at 0).
 * - Returns 0 = valid/accept, 1 = need more bytes, 12 = reject (invalid).
 */
#define UTF8_ACCEPT 0
#define UTF8_REJECT 12

static const uint8_t utf8d[] = {
    /* Character class lookup: maps byte values 0-255 to classes 0-11 */
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
     7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
     8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

    /* State transition table: state * 12 + class -> new state */
     0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
    12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
    12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
    12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
    12,36,12,12,12,12,12,12,12,12,12,12,
};

static uint32_t utf8_validate(uint32_t state, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint32_t type = utf8d[data[i]];
        state = utf8d[256 + state + type];
        if (state == UTF8_REJECT)
            return UTF8_REJECT;
    }
    return state;
}

/* Check if a close code is valid per RFC 6455 Section 7.4.1 */
static int is_valid_close_code(uint16_t code) {
    if (code >= 3000 && code <= 4999) return 1;
    switch (code) {
    case 1000: case 1001: case 1002: case 1003:
    case 1007: case 1008: case 1009: case 1010: case 1011:
        return 1;
    default:
        return 0;
    }
}

int wl_ws_handshake(wl_ws_conn_t *conn, const char *host, const char *path) {
    /* Generate random 16-byte key */
    uint8_t key_bytes[16];
    if (wl_platform_random(key_bytes, sizeof(key_bytes)) != 0)
        return -1;

    char *key_b64 = wl_base64_encode(key_bytes, sizeof(key_bytes));
    if (!key_b64) return -1;

    /* Compute expected accept value */
    wl_buf_t accept_input;
    wl_buf_init(&accept_input);
    wl_buf_append_str(&accept_input, key_b64);
    wl_buf_append_str(&accept_input, WS_GUID);

    uint8_t sha1_hash[WL_SHA1_DIGEST_SIZE];
    wl_sha1(accept_input.data, accept_input.len, sha1_hash);
    wl_buf_free(&accept_input);

    char *expected_accept = wl_base64_encode(sha1_hash, WL_SHA1_DIGEST_SIZE);

    /* Build upgrade request */
    wl_buf_t req;
    wl_buf_init(&req);
    wl_buf_append_fmt(&req, "GET %s HTTP/1.1\r\n", path);
    wl_buf_append_fmt(&req, "Host: %s\r\n", host);
    wl_buf_append_str(&req, "Upgrade: websocket\r\n");
    wl_buf_append_str(&req, "Connection: Upgrade\r\n");
    wl_buf_append_fmt(&req, "Sec-WebSocket-Key: %s\r\n", key_b64);
    wl_buf_append_str(&req, "Sec-WebSocket-Version: 13\r\n");
#ifdef WEBLITE_HAS_COMPRESSION
    wl_buf_append_str(&req,
        "Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits\r\n");
#endif
    wl_buf_append_str(&req, "\r\n");

    free(key_b64);

    int rc = wl_ws_raw_send(conn, req.data, req.len);
    wl_buf_free(&req);
    if (rc < 0) {
        free(expected_accept);
        return -1;
    }

    /* Read response (look for \r\n\r\n) */
    wl_buf_t resp_buf;
    wl_buf_init(&resp_buf);
    char tmp[1];
    int found = 0;

    while (resp_buf.len < 8192) {
        int n = wl_ws_raw_recv(conn, tmp, 1);
        if (n <= 0) break;
        wl_buf_append(&resp_buf, tmp, 1);

        if (resp_buf.len >= 4) {
            const char *end = resp_buf.data + resp_buf.len - 4;
            if (end[0] == '\r' && end[1] == '\n' && end[2] == '\r' && end[3] == '\n') {
                found = 1;
                break;
            }
        }
    }

    if (!found) {
        wl_buf_free(&resp_buf);
        free(expected_accept);
        return -1;
    }

    /* Null-terminate for safe strstr usage */
    wl_buf_append_char(&resp_buf, '\0');
    resp_buf.len--; /* don't count NUL in length */

    /* Verify 101 status */
    int status = 0;
    if (resp_buf.len > 12) {
        /* "HTTP/1.1 101" */
        const char *sp = memchr(resp_buf.data, ' ', resp_buf.len);
        if (sp) status = (int)strtol(sp + 1, NULL, 10);
    }

    if (status != 101) {
        wl_buf_free(&resp_buf);
        free(expected_accept);
        return -1;
    }

    /* Verify Sec-WebSocket-Accept (optional but recommended) */
    if (expected_accept) {
        /* Simple search for the accept value in response headers */
        char *accept_hdr = strstr(resp_buf.data, "Sec-WebSocket-Accept: ");
        if (!accept_hdr)
            accept_hdr = strstr(resp_buf.data, "sec-websocket-accept: ");
        if (accept_hdr) {
            accept_hdr += strlen("Sec-WebSocket-Accept: ");
            size_t accept_len = strlen(expected_accept);
            if (strncmp(accept_hdr, expected_accept, accept_len) != 0) {
                wl_buf_free(&resp_buf);
                free(expected_accept);
                return -1;
            }
        }
    }

#ifdef WEBLITE_HAS_COMPRESSION
    /* Check for permessage-deflate in response extensions */
    {
        const char *ext = resp_buf.data;
        /* Case-insensitive search for the extension header */
        while (ext && ext < resp_buf.data + resp_buf.len) {
            ext = strstr(ext, "ec-WebSocket-Extensions:");
            if (!ext) break;
            /* Back up one char to check 'S' */
            if (ext > resp_buf.data && (*(ext - 1) == 'S' || *(ext - 1) == 's')) {
                ext += strlen("ec-WebSocket-Extensions:");
                /* Find end of header value (next \r\n) */
                const char *eol = strstr(ext, "\r\n");
                if (!eol) eol = resp_buf.data + resp_buf.len;
                size_t vlen = (size_t)(eol - ext);
                char *val = malloc(vlen + 1);
                if (val) {
                    memcpy(val, ext, vlen);
                    val[vlen] = '\0';
                    if (strstr(val, "permessage-deflate")) {
                        conn->compress_enabled = 1;
                        conn->server_no_context_takeover =
                            (strstr(val, "server_no_context_takeover") != NULL);
                        conn->client_no_context_takeover =
                            (strstr(val, "client_no_context_takeover") != NULL);
                        /* Parse server_max_window_bits */
                        const char *smwb = strstr(val, "server_max_window_bits");
                        if (smwb) {
                            smwb += strlen("server_max_window_bits");
                            while (*smwb == ' ' || *smwb == '=') smwb++;
                            int bits = (int)strtol(smwb, NULL, 10);
                            if (bits >= 8 && bits <= 15)
                                conn->server_max_window_bits = bits;
                        }
                        /* Parse client_max_window_bits */
                        const char *cmwb = strstr(val, "client_max_window_bits");
                        if (cmwb) {
                            cmwb += strlen("client_max_window_bits");
                            while (*cmwb == ' ' || *cmwb == '=') cmwb++;
                            int bits = (int)strtol(cmwb, NULL, 10);
                            if (bits >= 8 && bits <= 15)
                                conn->client_max_window_bits = bits;
                        }
                    }
                    free(val);
                }
                break;
            }
            ext++;
        }
        if (conn->compress_enabled) {
            if (ws_deflate_init(conn) != 0 || ws_inflate_init(conn) != 0) {
                wl_ws_compression_cleanup(conn);
                conn->compress_enabled = 0;
            }
        }
    }
#endif

    wl_buf_free(&resp_buf);
    free(expected_accept);
    return 0;
}

int wl_ws_send_frame(wl_ws_conn_t *conn, wl_ws_opcode_t opcode,
                     const void *data, size_t len) {
    uint8_t header[14];
    size_t header_len = 0;

#ifdef WEBLITE_HAS_COMPRESSION
    void *compressed = NULL;
    size_t comp_len = 0;
    int is_data = (opcode == WL_WS_OP_TEXT || opcode == WL_WS_OP_BINARY);
    int do_compress = (is_data && conn->compress_enabled && conn->deflate_ctx);

    if (do_compress) {
        if (ws_compress(conn, data, len, &compressed, &comp_len) != 0) {
            /* Compression failed — send uncompressed */
            do_compress = 0;
        } else {
            data = compressed;
            len = comp_len;
        }
    }
#endif

    /* FIN + opcode (+ RSV1 if compressed) */
    header[0] = 0x80 | (uint8_t)opcode;
#ifdef WEBLITE_HAS_COMPRESSION
    if (do_compress)
        header[0] |= 0x40; /* RSV1 = 1 */
#endif
    header_len = 1;

    /* Payload length + mask bit (clients MUST mask) */
    if (len < 126) {
        header[1] = 0x80 | (uint8_t)len;
        header_len = 2;
    } else if (len <= 0xFFFF) {
        header[1] = 0x80 | 126;
        header[2] = (uint8_t)(len >> 8);
        header[3] = (uint8_t)(len);
        header_len = 4;
    } else {
        header[1] = 0x80 | 127;
        header[2] = (uint8_t)((uint64_t)len >> 56);
        header[3] = (uint8_t)((uint64_t)len >> 48);
        header[4] = (uint8_t)((uint64_t)len >> 40);
        header[5] = (uint8_t)((uint64_t)len >> 32);
        header[6] = (uint8_t)(len >> 24);
        header[7] = (uint8_t)(len >> 16);
        header[8] = (uint8_t)(len >> 8);
        header[9] = (uint8_t)(len);
        header_len = 10;
    }

    /* Generate masking key */
    uint8_t mask[4];
    wl_platform_random(mask, 4);
    memcpy(header + header_len, mask, 4);
    header_len += 4;

    /* Send header */
    if (wl_ws_raw_send(conn, header, header_len) < 0)
        return -1;

    /* Mask and send payload */
    if (len > 0) {
        uint8_t *masked = malloc(len);
        if (!masked) return -1;
        const uint8_t *src = data;
        for (size_t i = 0; i < len; i++)
            masked[i] = src[i] ^ mask[i % 4];

        int rc = wl_ws_raw_send(conn, masked, len);
        free(masked);
        if (rc < 0) {
#ifdef WEBLITE_HAS_COMPRESSION
            free(compressed);
#endif
            return -1;
        }
    }

#ifdef WEBLITE_HAS_COMPRESSION
    free(compressed);
#endif
    return 0;
}

int wl_ws_recv_frame(wl_ws_conn_t *conn, wl_ws_opcode_t *out_opcode,
                     void **out_data, size_t *out_len) {
    wl_buf_t payload;
    wl_buf_init(&payload);
    int first_opcode = -1;
    uint32_t utf8_state = UTF8_ACCEPT;
    int is_text = 0;
    int msg_compressed = 0;
    (void)msg_compressed;

    while (1) {
        uint8_t hdr[2];
        if (recv_exact(conn, hdr, 2) != 0) goto fail;

        int fin = (hdr[0] >> 7) & 1;
        int opcode = hdr[0] & 0x0F;
        int masked = (hdr[1] >> 7) & 1;
        uint64_t payload_len = hdr[1] & 0x7F;

        /* RSV bit validation — RFC 6455 §5.2 / RFC 7692 */
        int rsv1 = (hdr[0] >> 6) & 1;
        int rsv23 = hdr[0] & 0x30;
        if (rsv23) {
            /* RSV2 or RSV3 set — always invalid */
            send_close_code(conn, 1002);
            goto fail;
        }
        if (rsv1) {
#ifdef WEBLITE_HAS_COMPRESSION
            if (!conn->compress_enabled) {
                send_close_code(conn, 1002);
                goto fail;
            }
#else
            send_close_code(conn, 1002);
            goto fail;
#endif
        }

        /* Reject reserved opcodes — RFC 6455 §5.2 */
        int is_control = (opcode >= 0x8);
        if ((opcode >= 3 && opcode <= 7) || (opcode >= 0xB)) {
            send_close_code(conn, 1002);
            goto fail;
        }

        /* Control frames MUST NOT be fragmented — RFC 6455 §5.5 */
        if (is_control && !fin) {
            send_close_code(conn, 1002);
            goto fail;
        }

        /* Read extended payload length */
        if (payload_len == 126) {
            uint8_t ext[2];
            if (recv_exact(conn, ext, 2) != 0) goto fail;
            payload_len = ((uint64_t)ext[0] << 8) | ext[1];
        } else if (payload_len == 127) {
            uint8_t ext[8];
            if (recv_exact(conn, ext, 8) != 0) goto fail;
            payload_len = 0;
            for (int i = 0; i < 8; i++)
                payload_len = (payload_len << 8) | ext[i];
        }

        /* Control frame payload MUST NOT exceed 125 bytes — RFC 6455 §5.5 */
        if (is_control && payload_len > 125) {
            send_close_code(conn, 1002);
            goto fail;
        }

        uint8_t mask[4] = {0};
        if (masked) {
            if (recv_exact(conn, mask, 4) != 0) goto fail;
        }

        /* Read payload */
        uint8_t *frame_data = NULL;
        if (payload_len > 0) {
            if (payload_len > 16 * 1024 * 1024) goto fail; /* 16MB limit */
            frame_data = malloc((size_t)payload_len);
            if (!frame_data) goto fail;
            if (recv_exact(conn, frame_data, (size_t)payload_len) != 0) {
                free(frame_data);
                goto fail;
            }
            if (masked) {
                for (size_t i = 0; i < payload_len; i++)
                    frame_data[i] ^= mask[i % 4];
            }
        }

        /* Handle control frames (interleaved with data fragments) */
        if (opcode == WL_WS_OP_PING) {
            wl_ws_send_frame(conn, WL_WS_OP_PONG, frame_data, (size_t)payload_len);
            free(frame_data);
            continue;
        } else if (opcode == WL_WS_OP_PONG) {
            free(frame_data);
            continue;
        } else if (opcode == WL_WS_OP_CLOSE) {
            /* Validate and echo close frame — RFC 6455 §7.4 */
            if (payload_len == 1) {
                /* Close payload must be 0 or >= 2 bytes */
                send_close_code(conn, 1002);
                free(frame_data);
                goto fail;
            }
            if (payload_len >= 2 && frame_data) {
                uint16_t code = (uint16_t)((frame_data[0] << 8) | frame_data[1]);
                if (!is_valid_close_code(code)) {
                    send_close_code(conn, 1002);
                    free(frame_data);
                    goto fail;
                }
                /* Validate close reason is valid UTF-8 */
                if (payload_len > 2) {
                    uint32_t s = utf8_validate(UTF8_ACCEPT, frame_data + 2,
                                               (size_t)payload_len - 2);
                    if (s != UTF8_ACCEPT) {
                        send_close_code(conn, 1007);
                        free(frame_data);
                        goto fail;
                    }
                }
                /* Echo back just the status code */
                send_close_code(conn, code);
            } else {
                /* Empty close frame — echo empty close */
                wl_ws_send_frame(conn, WL_WS_OP_CLOSE, NULL, 0);
            }
            free(frame_data);
            goto fail;
        }

        /* --- Data frame fragmentation protocol — RFC 6455 §5.4 --- */
        if (opcode == WL_WS_OP_CONTINUATION) {
            /* Continuation without a started message */
            if (first_opcode < 0) {
                send_close_code(conn, 1002);
                free(frame_data);
                goto fail;
            }
        } else {
            /* New data opcode while mid-message (expected continuation) */
            if (first_opcode >= 0) {
                send_close_code(conn, 1002);
                free(frame_data);
                goto fail;
            }
            first_opcode = opcode;
            is_text = (opcode == WL_WS_OP_TEXT);
            /* RSV1 on first frame means the message is compressed */
            if (rsv1)
                msg_compressed = 1;
        }

        if (frame_data && payload_len > 0) {
            /* Incremental UTF-8 validation for text messages
               (skip if compressed — bytes aren't valid UTF-8 yet) */
            if (is_text && !msg_compressed) {
                utf8_state = utf8_validate(utf8_state, frame_data,
                                           (size_t)payload_len);
                if (utf8_state == UTF8_REJECT) {
                    send_close_code(conn, 1007);
                    free(frame_data);
                    goto fail;
                }
            }
            wl_buf_append(&payload, frame_data, (size_t)payload_len);
            free(frame_data);
        }

        if (fin) {
#ifdef WEBLITE_HAS_COMPRESSION
            /* Decompress assembled payload if message was compressed */
            if (msg_compressed && conn->compress_enabled && conn->inflate_ctx) {
                wl_buf_t decompressed;
                wl_buf_init(&decompressed);
                if (ws_decompress(conn, payload.data, payload.len,
                                  &decompressed) != 0) {
                    wl_buf_free(&decompressed);
                    send_close_code(conn, 1007);
                    goto fail;
                }
                wl_buf_free(&payload);
                payload = decompressed;
            }
#endif
            /* UTF-8 validation on final (possibly decompressed) payload */
            if (is_text) {
                if (msg_compressed) {
                    /* Validate entire decompressed payload */
                    utf8_state = utf8_validate(UTF8_ACCEPT,
                                               (const uint8_t *)payload.data,
                                               payload.len);
                }
                if (utf8_state != UTF8_ACCEPT) {
                    send_close_code(conn, 1007);
                    goto fail;
                }
            }
            break;
        }
    }

    *out_opcode = (wl_ws_opcode_t)first_opcode;
    *out_len = payload.len;
    *out_data = wl_buf_detach(&payload, NULL);
    return 0;

fail:
    wl_buf_free(&payload);
    *out_data = NULL;
    *out_len = 0;
    return -1;
}

void wl_ws_close(wl_ws_conn_t *conn) {
    /* Send close frame with status 1000 (normal) */
    uint8_t close_payload[2] = { 0x03, 0xE8 }; /* 1000 in big-endian */
    wl_ws_send_frame(conn, WL_WS_OP_CLOSE, close_payload, 2);
}

/* Simple JSON builder/parser for REST-over-WS tunneling */

static int json_escape_append(wl_buf_t *buf, const char *str) {
    wl_buf_append_char(buf, '"');
    for (const char *p = str; *p; p++) {
        switch (*p) {
        case '"':  wl_buf_append_str(buf, "\\\""); break;
        case '\\': wl_buf_append_str(buf, "\\\\"); break;
        case '\n': wl_buf_append_str(buf, "\\n"); break;
        case '\r': wl_buf_append_str(buf, "\\r"); break;
        case '\t': wl_buf_append_str(buf, "\\t"); break;
        default:   wl_buf_append_char(buf, *p); break;
        }
    }
    wl_buf_append_char(buf, '"');
    return 0;
}

static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static const char *parse_json_string(const char *p, char **out) {
    if (*p != '"') return NULL;
    p++;
    wl_buf_t buf;
    wl_buf_init(&buf);
    while (*p && *p != '"') {
        if (*p == '\\') {
            p++;
            switch (*p) {
            case '"':  wl_buf_append_char(&buf, '"'); break;
            case '\\': wl_buf_append_char(&buf, '\\'); break;
            case 'n':  wl_buf_append_char(&buf, '\n'); break;
            case 'r':  wl_buf_append_char(&buf, '\r'); break;
            case 't':  wl_buf_append_char(&buf, '\t'); break;
            default:   wl_buf_append_char(&buf, *p); break;
            }
        } else {
            wl_buf_append_char(&buf, *p);
        }
        p++;
    }
    if (*p == '"') p++;
    *out = wl_buf_detach(&buf, NULL);
    return p;
}

int wl_ws_rest_request(wl_ws_conn_t *conn,
                       weblite_method_t method,
                       const char *path,
                       const weblite_header_t *headers,
                       size_t header_count,
                       const void *body,
                       size_t body_len,
                       weblite_response_t *resp) {
    const char *method_str = wl_http_method_str(method);

    /* Build JSON request */
    wl_buf_t json;
    wl_buf_init(&json);

    wl_buf_append_str(&json, "{\"method\":");
    json_escape_append(&json, method_str);
    wl_buf_append_str(&json, ",\"path\":");
    json_escape_append(&json, path);

    /* Headers */
    wl_buf_append_str(&json, ",\"headers\":{");
    for (size_t i = 0; i < header_count; i++) {
        if (i > 0) wl_buf_append_char(&json, ',');
        json_escape_append(&json, headers[i].name);
        wl_buf_append_char(&json, ':');
        json_escape_append(&json, headers[i].value);
    }
    wl_buf_append_str(&json, "}");

    /* Body */
    wl_buf_append_str(&json, ",\"body\":");
    if (body && body_len > 0) {
        /* Base64 encode binary body */
        char *b64 = wl_base64_encode(body, body_len);
        if (b64) {
            json_escape_append(&json, b64);
            free(b64);
        } else {
            json_escape_append(&json, "");
        }
    } else {
        json_escape_append(&json, "");
    }

    wl_buf_append_char(&json, '}');

    /* Send as text frame */
    int rc = wl_ws_send_frame(conn, WL_WS_OP_TEXT, json.data, json.len);
    wl_buf_free(&json);
    if (rc != 0) return -1;

    /* Receive response frame */
    wl_ws_opcode_t opcode;
    void *frame_data;
    size_t frame_len;
    if (wl_ws_recv_frame(conn, &opcode, &frame_data, &frame_len) != 0)
        return -1;

    if (opcode != WL_WS_OP_TEXT) {
        free(frame_data);
        return -1;
    }

    /* Parse JSON response: {"status":200,"headers":{...},"body":"..."} */
    const char *p = frame_data;
    p = skip_ws(p);
    if (*p != '{') { free(frame_data); return -1; }
    p++;

    resp->status_code = 0;
    resp->headers = NULL;
    resp->header_count = 0;
    resp->body = NULL;
    resp->body_len = 0;

    while (*p && *p != '}') {
        p = skip_ws(p);
        if (*p == ',') p++;
        p = skip_ws(p);

        char *key = NULL;
        p = parse_json_string(p, &key);
        if (!p || !key) { free(frame_data); return -1; }

        p = skip_ws(p);
        if (*p == ':') p++;
        p = skip_ws(p);

        if (strcmp(key, "status") == 0) {
            resp->status_code = (int)strtol(p, (char **)&p, 10);
        } else if (strcmp(key, "body") == 0) {
            char *body_str = NULL;
            p = parse_json_string(p, &body_str);
            if (body_str) {
                resp->body = body_str;
                resp->body_len = strlen(body_str);
            }
        } else if (strcmp(key, "headers") == 0) {
            /* Parse headers object */
            if (*p == '{') {
                p++;
                size_t max_h = 32;
                resp->headers = calloc(max_h, sizeof(weblite_header_owned_t));
                while (*p && *p != '}') {
                    p = skip_ws(p);
                    if (*p == ',') p++;
                    p = skip_ws(p);
                    if (*p == '}') break;

                    char *hname = NULL, *hval = NULL;
                    p = parse_json_string(p, &hname);
                    if (!p) break;
                    p = skip_ws(p);
                    if (*p == ':') p++;
                    p = skip_ws(p);
                    p = parse_json_string(p, &hval);
                    if (!p) { free(hname); break; }

                    if (resp->header_count >= max_h) {
                        max_h *= 2;
                        resp->headers = realloc(resp->headers,
                            max_h * sizeof(weblite_header_owned_t));
                    }
                    resp->headers[resp->header_count].name = hname;
                    resp->headers[resp->header_count].value = hval;
                    resp->header_count++;
                }
                if (*p == '}') p++;
            }
        } else {
            /* Skip unknown value */
            if (*p == '"') {
                char *tmp = NULL;
                p = parse_json_string(p, &tmp);
                free(tmp);
            } else {
                while (*p && *p != ',' && *p != '}') p++;
            }
        }

        free(key);
    }

    free(frame_data);
    return 0;
}

/* --- transport.c --- */

/* Send all data through the connection (plain or TLS) */
static int conn_send(wl_socket_t sock, void *tls_ctx, int is_tls,
                     const void *data, size_t len) {
#ifdef WEBLITE_HAS_TLS
    if (is_tls && tls_ctx)
        return wl_tls_send(tls_ctx, data, len);
#endif
    (void)tls_ctx; (void)is_tls;
    return wl_platform_socket_send(sock, data, len);
}

/* Receive data from connection (plain or TLS) */
static int conn_recv(wl_socket_t sock, void *tls_ctx, int is_tls,
                     void *buf, size_t len) {
#ifdef WEBLITE_HAS_TLS
    if (is_tls && tls_ctx)
        return wl_tls_recv(tls_ctx, buf, len);
#endif
    (void)tls_ctx; (void)is_tls;
    return wl_platform_socket_recv(sock, buf, len);
}

static void set_error(weblite_response_t *resp, weblite_error_t err, const char *msg) {
    resp->error = err;
    if (msg)
        snprintf(resp->error_msg, sizeof(resp->error_msg), "%s", msg);
}

static weblite_error_t do_http_request(const weblite_request_t *req,
                                       weblite_response_t *resp,
                                       uint32_t timeout_ms,
                                       uint32_t max_redirects,
                                       int tls_verify,
                                       const char *ca_path) {
    uint32_t redirects = 0;
    char *current_url = NULL;

    /* Copy URL so we can update on redirect */
    current_url = strdup(req->url);
    if (!current_url) {
        set_error(resp, WEBLITE_ERR_ALLOC, "URL allocation failed");
        return WEBLITE_ERR_ALLOC;
    }

    weblite_error_t result = WEBLITE_OK;

    while (1) {
        wl_url_t url;
        if (wl_url_parse(current_url, &url) != 0) {
            set_error(resp, WEBLITE_ERR_INVALID_ARG, "Invalid URL");
            result = WEBLITE_ERR_INVALID_ARG;
            break;
        }

        uint16_t port = wl_url_default_port(&url);

        /* Connect */
        wl_socket_t sock = wl_platform_socket_open(url.host, port, timeout_ms);
        if (sock == WL_INVALID_SOCKET) {
            set_error(resp, WEBLITE_ERR_CONNECT, "Connection failed");
            wl_url_free(&url);
            result = WEBLITE_ERR_CONNECT;
            break;
        }

        if (timeout_ms > 0)
            wl_platform_socket_set_timeout(sock, timeout_ms);

        /* TLS if needed */
        void *tls_ctx = NULL;
#ifdef WEBLITE_HAS_TLS
        if (url.is_tls) {
            tls_ctx = wl_tls_connect(sock, url.host, tls_verify, ca_path);
            if (!tls_ctx) {
                set_error(resp, WEBLITE_ERR_TLS, "TLS handshake failed");
                wl_platform_socket_close(sock);
                wl_url_free(&url);
                result = WEBLITE_ERR_TLS;
                break;
            }
        }
#else
        (void)tls_verify; (void)ca_path;
        if (url.is_tls) {
            set_error(resp, WEBLITE_ERR_TLS, "TLS not enabled in this build");
            wl_platform_socket_close(sock);
            wl_url_free(&url);
            result = WEBLITE_ERR_TLS;
            break;
        }
#endif

        /* Build path with query */
        wl_buf_t path_buf;
        wl_buf_init(&path_buf);
        wl_buf_append_str(&path_buf, url.path);
        if (url.query) {
            wl_buf_append_char(&path_buf, '?');
            wl_buf_append_str(&path_buf, url.query);
        }
        wl_buf_append_char(&path_buf, '\0');

        /* Build host header value (include port if non-default) */
        char host_header[256];
        if ((url.is_tls && port != 443) || (!url.is_tls && port != 80))
            snprintf(host_header, sizeof(host_header), "%s:%u", url.host, port);
        else
            snprintf(host_header, sizeof(host_header), "%s", url.host);

        /* Build HTTP request */
        wl_buf_t req_buf;
        wl_buf_init(&req_buf);
        int build_rc = wl_http_build_request(&req_buf, req->method,
                                              host_header, path_buf.data,
                                              req->headers, req->header_count,
                                              req->body, req->body_len);
        wl_buf_free(&path_buf);

        if (build_rc != 0) {
            set_error(resp, WEBLITE_ERR_ALLOC, "Failed to build request");
            wl_buf_free(&req_buf);
#ifdef WEBLITE_HAS_TLS
            if (tls_ctx) wl_tls_close(tls_ctx);
#endif
            wl_platform_socket_close(sock);
            wl_url_free(&url);
            result = WEBLITE_ERR_ALLOC;
            break;
        }

        /* Send request */
        int send_rc = conn_send(sock, tls_ctx, url.is_tls, req_buf.data, req_buf.len);
        wl_buf_free(&req_buf);

        if (send_rc < 0) {
            set_error(resp, WEBLITE_ERR_IO, "Send failed");
#ifdef WEBLITE_HAS_TLS
            if (tls_ctx) wl_tls_close(tls_ctx);
#endif
            wl_platform_socket_close(sock);
            wl_url_free(&url);
            result = WEBLITE_ERR_IO;
            break;
        }

        /* Receive response */
        wl_buf_t recv_buf;
        wl_buf_init(&recv_buf);
        char tmp[4096];
        size_t body_offset = 0;
        int headers_parsed = 0;

        while (1) {
            int n = conn_recv(sock, tls_ctx, url.is_tls, tmp, sizeof(tmp));
            if (n < 0) {
                set_error(resp, WEBLITE_ERR_IO, "Receive failed");
                result = WEBLITE_ERR_IO;
                break;
            }
            if (n == 0)
                break; /* Connection closed */

            wl_buf_append(&recv_buf, tmp, (size_t)n);

            if (!headers_parsed) {
                int parse_rc = wl_http_parse_response(recv_buf.data, recv_buf.len,
                                                       resp, &body_offset);
                if (parse_rc == 0) {
                    headers_parsed = 1;

                    /* Check if we have the full body */
                    int64_t content_length = wl_http_get_content_length(
                        resp->headers, resp->header_count);

                    if (content_length >= 0) {
                        size_t body_received = recv_buf.len - body_offset;
                        if (body_received >= (size_t)content_length)
                            break;
                    }
                    /* If chunked or no content-length, keep reading until connection close */
                } else if (parse_rc < 0) {
                    set_error(resp, WEBLITE_ERR_HTTP_PARSE, "HTTP parse error");
                    result = WEBLITE_ERR_HTTP_PARSE;
                    break;
                }
            } else {
                /* Check if body complete */
                int64_t content_length = wl_http_get_content_length(
                    resp->headers, resp->header_count);
                if (content_length >= 0) {
                    size_t body_received = recv_buf.len - body_offset;
                    if (body_received >= (size_t)content_length)
                        break;
                }
            }
        }

#ifdef WEBLITE_HAS_TLS
        if (tls_ctx) wl_tls_close(tls_ctx);
#endif
        wl_platform_socket_close(sock);

        if (result != WEBLITE_OK) {
            wl_buf_free(&recv_buf);
            wl_url_free(&url);
            break;
        }

        if (!headers_parsed) {
            /* We got EOF before headers were complete */
            if (recv_buf.len > 0) {
                int parse_rc = wl_http_parse_response(recv_buf.data, recv_buf.len,
                                                       resp, &body_offset);
                if (parse_rc != 0) {
                    set_error(resp, WEBLITE_ERR_HTTP_PARSE, "Incomplete HTTP response");
                    wl_buf_free(&recv_buf);
                    wl_url_free(&url);
                    result = WEBLITE_ERR_HTTP_PARSE;
                    break;
                }
            } else {
                set_error(resp, WEBLITE_ERR_IO, "Empty response");
                wl_buf_free(&recv_buf);
                wl_url_free(&url);
                result = WEBLITE_ERR_IO;
                break;
            }
        }

        /* Extract body */
        size_t raw_body_len = recv_buf.len - body_offset;
        if (wl_http_is_chunked(resp->headers, resp->header_count)) {
            size_t decoded_len;
            resp->body = wl_http_decode_chunked(recv_buf.data + body_offset,
                                                 raw_body_len, &decoded_len);
            resp->body_len = decoded_len;
        } else {
            int64_t cl = wl_http_get_content_length(resp->headers, resp->header_count);
            size_t body_len = (cl >= 0) ? (size_t)cl : raw_body_len;
            if (body_len > raw_body_len) body_len = raw_body_len;

            if (body_len > 0) {
                resp->body = malloc(body_len + 1);
                if (resp->body) {
                    memcpy(resp->body, recv_buf.data + body_offset, body_len);
                    ((char *)resp->body)[body_len] = '\0';
                    resp->body_len = body_len;
                }
            }
        }

        wl_buf_free(&recv_buf);
        resp->transport_used = WEBLITE_USED_HTTP;

        /* Handle redirects */
        if (resp->status_code >= 300 && resp->status_code < 400 &&
            max_redirects > 0 && redirects < max_redirects) {

            const char *location = wl_http_get_location(resp->headers,
                                                         resp->header_count);
            if (location) {
                /* Copy location before freeing headers (location points into resp->headers) */
                char *loc_copy = strdup(location);

                /* Free current response data for redirect */
                free(resp->body);
                resp->body = NULL;
                resp->body_len = 0;
                for (size_t i = 0; i < resp->header_count; i++) {
                    free(resp->headers[i].name);
                    free(resp->headers[i].value);
                }
                free(resp->headers);
                resp->headers = NULL;
                resp->header_count = 0;

                if (!loc_copy) {
                    set_error(resp, WEBLITE_ERR_ALLOC, "Redirect URL allocation failed");
                    wl_url_free(&url);
                    result = WEBLITE_ERR_ALLOC;
                    break;
                }

                /* Build new URL */
                if (loc_copy[0] == '/') {
                    /* Relative redirect */
                    char *new_url = NULL;
                    int n = snprintf(NULL, 0, "%s://%s%s", url.scheme, url.host, loc_copy);
                    new_url = malloc((size_t)n + 1);
                    if (new_url)
                        snprintf(new_url, (size_t)n + 1, "%s://%s%s",
                                 url.scheme, url.host, loc_copy);
                    free(current_url);
                    current_url = new_url;
                } else {
                    free(current_url);
                    current_url = strdup(loc_copy);
                }
                free(loc_copy);

                wl_url_free(&url);
                redirects++;

                if (!current_url) {
                    set_error(resp, WEBLITE_ERR_ALLOC, "Redirect URL allocation failed");
                    result = WEBLITE_ERR_ALLOC;
                    break;
                }
                continue; /* Retry with new URL */
            }
        }

        wl_url_free(&url);
        break;
    }

    free(current_url);
    return result;
}

static weblite_error_t do_ws_request(const weblite_request_t *req,
                                     weblite_response_t *resp,
                                     uint32_t timeout_ms,
                                     int tls_verify,
                                     const char *ca_path) {
    wl_url_t url;
    if (wl_url_parse(req->url, &url) != 0) {
        set_error(resp, WEBLITE_ERR_INVALID_ARG, "Invalid URL");
        return WEBLITE_ERR_INVALID_ARG;
    }

    uint16_t port = wl_url_default_port(&url);

    wl_socket_t sock = wl_platform_socket_open(url.host, port, timeout_ms);
    if (sock == WL_INVALID_SOCKET) {
        set_error(resp, WEBLITE_ERR_CONNECT, "WS connection failed");
        wl_url_free(&url);
        return WEBLITE_ERR_CONNECT;
    }

    if (timeout_ms > 0)
        wl_platform_socket_set_timeout(sock, timeout_ms);

    wl_ws_conn_t ws_conn;
    ws_conn.sock = sock;
    ws_conn.is_tls = url.is_tls;
#ifdef WEBLITE_HAS_TLS
    ws_conn.tls = NULL;
    if (url.is_tls) {
        ws_conn.tls = wl_tls_connect(sock, url.host, tls_verify, ca_path);
        if (!ws_conn.tls) {
            set_error(resp, WEBLITE_ERR_TLS, "WS TLS handshake failed");
            wl_platform_socket_close(sock);
            wl_url_free(&url);
            return WEBLITE_ERR_TLS;
        }
    }
#else
    (void)tls_verify; (void)ca_path;
#endif

    /* WebSocket handshake */
    char *ws_path = url.path ? url.path : "/";
    if (wl_ws_handshake(&ws_conn, url.host, ws_path) != 0) {
        set_error(resp, WEBLITE_ERR_WS_HANDSHAKE, "WebSocket handshake failed");
#ifdef WEBLITE_HAS_TLS
        if (ws_conn.tls) wl_tls_close(ws_conn.tls);
#endif
        wl_platform_socket_close(sock);
        wl_url_free(&url);
        return WEBLITE_ERR_WS_HANDSHAKE;
    }

    /* Send REST-over-WS request */
    int rc = wl_ws_rest_request(&ws_conn, req->method, url.path,
                                 req->headers, req->header_count,
                                 req->body, req->body_len, resp);

    resp->transport_used = WEBLITE_USED_WS;

    wl_ws_close(&ws_conn);
#ifdef WEBLITE_HAS_TLS
    if (ws_conn.tls) wl_tls_close(ws_conn.tls);
#endif
    wl_platform_socket_close(sock);
    wl_url_free(&url);

    if (rc != 0) {
        set_error(resp, WEBLITE_ERR_WS_PROTOCOL, "WS REST request failed");
        return WEBLITE_ERR_WS_PROTOCOL;
    }

    return WEBLITE_OK;
}

weblite_error_t wl_transport_execute(const weblite_request_t *req,
                                     weblite_response_t *resp,
                                     weblite_transport_mode_t mode,
                                     uint32_t timeout_ms,
                                     uint32_t max_redirects,
                                     int tls_verify,
                                     const char *ca_path) {
    memset(resp, 0, sizeof(*resp));

    switch (mode) {
    case WEBLITE_TRANSPORT_HTTP_ONLY:
        return do_http_request(req, resp, timeout_ms, max_redirects,
                               tls_verify, ca_path);

    case WEBLITE_TRANSPORT_WS_ONLY:
        return do_ws_request(req, resp, timeout_ms, tls_verify, ca_path);

    case WEBLITE_TRANSPORT_AUTO:
    default: {
        /* Try HTTP first */
        weblite_error_t err = do_http_request(req, resp, timeout_ms,
                                               max_redirects, tls_verify, ca_path);
        if (err == WEBLITE_OK || err == WEBLITE_ERR_TLS ||
            err == WEBLITE_ERR_HTTP_PARSE || err == WEBLITE_ERR_INVALID_ARG)
            return err;

        /* HTTP connection failed, try WebSocket fallback */
        /* Free any partial response data from HTTP attempt */
        if (resp->headers) {
            for (size_t i = 0; i < resp->header_count; i++) {
                free(resp->headers[i].name);
                free(resp->headers[i].value);
            }
            free(resp->headers);
        }
        free(resp->body);
        memset(resp, 0, sizeof(*resp));

        return do_ws_request(req, resp, timeout_ms, tls_verify, ca_path);
    }
    }
}

/* --- async.c --- */

#ifdef WEBLITE_HAS_THREADS

/* Task in the queue */
typedef struct wl_async_task {
    weblite_request_t     req;
    char                 *url_copy;
    void                 *body_copy;
    weblite_header_t     *headers_copy;
    weblite_callback_t    callback;
    void                 *user_data;
    weblite_config_t      config;
    int                   cancelled;
    struct weblite_async_handle *handle;
    struct wl_async_task *next;
} wl_async_task_t;

/* Async handle */
struct weblite_async_handle {
    wl_async_task_t *task;
    wl_mutex_t       mutex;
    wl_cond_t        cond;
    int              done;
    weblite_error_t  result;
};

/* Thread pool state */
static wl_thread_t  *g_threads = NULL;
static int            g_thread_count = 0;
static wl_mutex_t     g_queue_mutex = NULL;
static wl_cond_t      g_queue_cond = NULL;
static wl_async_task_t *g_queue_head = NULL;
static wl_async_task_t *g_queue_tail = NULL;
static int            g_shutdown = 0;

static void task_free(wl_async_task_t *task) {
    if (!task) return;
    free(task->url_copy);
    free(task->body_copy);
    free(task->headers_copy);
    free(task);
}

static void *worker_thread(void *arg) {
    (void)arg;

    while (1) {
        wl_platform_mutex_lock(g_queue_mutex);

        while (!g_queue_head && !g_shutdown)
            wl_platform_cond_wait(g_queue_cond, g_queue_mutex);

        if (g_shutdown && !g_queue_head) {
            wl_platform_mutex_unlock(g_queue_mutex);
            break;
        }

        /* Dequeue task */
        wl_async_task_t *task = g_queue_head;
        g_queue_head = task->next;
        if (!g_queue_head)
            g_queue_tail = NULL;

        wl_platform_mutex_unlock(g_queue_mutex);

        /* Execute */
        weblite_response_t *resp = calloc(1, sizeof(weblite_response_t));
        weblite_error_t err = WEBLITE_ERR_ALLOC;

        if (resp && !task->cancelled) {
            weblite_transport_mode_t mode = task->req.transport_mode;
            if (mode == WEBLITE_TRANSPORT_AUTO)
                mode = task->config.default_transport;

            uint32_t timeout = task->req.timeout_ms > 0 ?
                task->req.timeout_ms : task->config.default_timeout_ms;
            uint32_t redirects = task->req.max_redirects > 0 ?
                task->req.max_redirects : task->config.default_max_redirects;

            err = wl_transport_execute(&task->req, resp, mode, timeout,
                                        redirects, task->config.tls_verify,
                                        task->config.ca_path);
            resp->error = err;
        } else if (task->cancelled) {
            err = WEBLITE_ERR_CANCELLED;
            if (resp) {
                resp->error = err;
                snprintf(resp->error_msg, sizeof(resp->error_msg), "Request cancelled");
            }
        }

        /* Invoke callback */
        if (task->callback && resp)
            task->callback(resp, task->user_data);

        /* Signal handle completion */
        if (task->handle) {
            wl_platform_mutex_lock(task->handle->mutex);
            task->handle->done = 1;
            task->handle->result = err;
            wl_platform_cond_signal(task->handle->cond);
            wl_platform_mutex_unlock(task->handle->mutex);
        }

        /* Free response */
        if (resp) {
            for (size_t i = 0; i < resp->header_count; i++) {
                free(resp->headers[i].name);
                free(resp->headers[i].value);
            }
            free(resp->headers);
            free(resp->body);
            free(resp);
        }

        task_free(task);
    }

    return NULL;
}

int wl_async_init(int thread_count) {
    if (thread_count <= 0) thread_count = 4;

    g_queue_mutex = wl_platform_mutex_create();
    g_queue_cond = wl_platform_cond_create();
    if (!g_queue_mutex || !g_queue_cond)
        return -1;

    g_thread_count = thread_count;
    g_threads = calloc((size_t)thread_count, sizeof(wl_thread_t));
    if (!g_threads)
        return -1;

    g_shutdown = 0;
    for (int i = 0; i < thread_count; i++) {
        g_threads[i] = wl_platform_thread_create(worker_thread, NULL);
        if (!g_threads[i]) {
            /* Cleanup already created threads */
            g_shutdown = 1;
            wl_platform_cond_broadcast(g_queue_cond);
            for (int j = 0; j < i; j++)
                wl_platform_thread_join(g_threads[j]);
            free(g_threads);
            g_threads = NULL;
            return -1;
        }
    }

    return 0;
}

void wl_async_cleanup(void) {
    if (!g_threads)
        return;

    /* Signal shutdown */
    wl_platform_mutex_lock(g_queue_mutex);
    g_shutdown = 1;
    wl_platform_cond_broadcast(g_queue_cond);
    wl_platform_mutex_unlock(g_queue_mutex);

    /* Join all threads */
    for (int i = 0; i < g_thread_count; i++) {
        if (g_threads[i])
            wl_platform_thread_join(g_threads[i]);
    }

    /* Free remaining tasks */
    while (g_queue_head) {
        wl_async_task_t *t = g_queue_head;
        g_queue_head = t->next;
        task_free(t);
    }

    free(g_threads);
    g_threads = NULL;
    g_thread_count = 0;

    wl_platform_mutex_destroy(g_queue_mutex);
    wl_platform_cond_destroy(g_queue_cond);
    g_queue_mutex = NULL;
    g_queue_cond = NULL;
}

weblite_async_handle_t *wl_async_submit(const weblite_request_t *req,
                                         weblite_callback_t callback,
                                         void *user_data,
                                         const weblite_config_t *config) {
    wl_async_task_t *task = calloc(1, sizeof(wl_async_task_t));
    if (!task)
        return NULL;

    /* Deep copy the request data */
    task->req = *req;
    task->url_copy = strdup(req->url);
    task->req.url = task->url_copy;

    if (req->body && req->body_len > 0) {
        task->body_copy = malloc(req->body_len);
        if (task->body_copy) {
            memcpy(task->body_copy, req->body, req->body_len);
            task->req.body = task->body_copy;
        }
    }

    if (req->headers && req->header_count > 0) {
        task->headers_copy = calloc(req->header_count, sizeof(weblite_header_t));
        if (task->headers_copy) {
            for (size_t i = 0; i < req->header_count; i++) {
                task->headers_copy[i].name = strdup(req->headers[i].name);
                task->headers_copy[i].value = strdup(req->headers[i].value);
            }
            task->req.headers = task->headers_copy;
        }
    }

    task->callback = callback;
    task->user_data = user_data;
    task->config = *config;

    /* Create handle */
    weblite_async_handle_t *handle = calloc(1, sizeof(weblite_async_handle_t));
    if (!handle) {
        task_free(task);
        return NULL;
    }

    handle->task = task;
    handle->mutex = wl_platform_mutex_create();
    handle->cond = wl_platform_cond_create();
    handle->done = 0;
    task->handle = handle;

    /* Enqueue */
    wl_platform_mutex_lock(g_queue_mutex);
    task->next = NULL;
    if (g_queue_tail)
        g_queue_tail->next = task;
    else
        g_queue_head = task;
    g_queue_tail = task;
    wl_platform_cond_signal(g_queue_cond);
    wl_platform_mutex_unlock(g_queue_mutex);

    return handle;
}

weblite_error_t wl_async_cancel(weblite_async_handle_t *handle) {
    if (!handle || !handle->task)
        return WEBLITE_ERR_INVALID_ARG;
    handle->task->cancelled = 1;
    return WEBLITE_OK;
}

weblite_error_t wl_async_wait(weblite_async_handle_t *handle, uint32_t timeout_ms) {
    if (!handle)
        return WEBLITE_ERR_INVALID_ARG;

    wl_platform_mutex_lock(handle->mutex);
    if (!handle->done) {
        if (timeout_ms > 0)
            wl_platform_cond_timedwait(handle->cond, handle->mutex, timeout_ms);
        else
            wl_platform_cond_wait(handle->cond, handle->mutex);
    }
    int done = handle->done;
    wl_platform_mutex_unlock(handle->mutex);

    return done ? WEBLITE_OK : WEBLITE_ERR_TIMEOUT;
}

int wl_async_poll(void) {
    return 0; /* Thread-based async doesn't need polling */
}

#else /* !WEBLITE_HAS_THREADS */

int  wl_async_init(int thread_count) { (void)thread_count; return 0; }
void wl_async_cleanup(void) { }

weblite_async_handle_t *wl_async_submit(const weblite_request_t *req,
                                         weblite_callback_t callback,
                                         void *user_data,
                                         const weblite_config_t *config) {
    (void)req; (void)callback; (void)user_data; (void)config;
    return NULL;
}

weblite_error_t wl_async_cancel(weblite_async_handle_t *handle) {
    (void)handle;
    return WEBLITE_ERR_UNSUPPORTED;
}

weblite_error_t wl_async_wait(weblite_async_handle_t *handle, uint32_t timeout_ms) {
    (void)handle; (void)timeout_ms;
    return WEBLITE_ERR_UNSUPPORTED;
}

int wl_async_poll(void) { return 0; }

#endif /* WEBLITE_HAS_THREADS */

/* --- platform_posix.c --- */

#ifdef WEBLITE_POSIX

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <poll.h>

#ifdef WEBLITE_HAS_THREADS
#include <pthread.h>
#endif

/* ------- Init / Cleanup ------- */

int wl_platform_init(void) {
    return 0; /* Nothing needed on POSIX */
}

void wl_platform_cleanup(void) {
    /* Nothing needed */
}

/* ------- Socket ------- */

wl_socket_t wl_platform_socket_open(const char *host, uint16_t port, uint32_t timeout_ms) {
    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", port);

    int rc = getaddrinfo(host, port_str, &hints, &result);
    if (rc != 0)
        return WL_INVALID_SOCKET;

    int fd = -1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0)
            continue;

        /* Non-blocking connect with timeout */
        if (timeout_ms > 0) {
            int flags = fcntl(fd, F_GETFL, 0);
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);

            rc = connect(fd, rp->ai_addr, rp->ai_addrlen);
            if (rc < 0 && errno == EINPROGRESS) {
                struct pollfd pfd = { .fd = fd, .events = POLLOUT };
                int poll_rc = poll(&pfd, 1, (int)timeout_ms);
                if (poll_rc <= 0) {
                    close(fd);
                    fd = -1;
                    continue;
                }
                int err = 0;
                socklen_t errlen = sizeof(err);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen);
                if (err != 0) {
                    close(fd);
                    fd = -1;
                    continue;
                }
            } else if (rc < 0) {
                close(fd);
                fd = -1;
                continue;
            }

            /* Restore blocking mode */
            fcntl(fd, F_SETFL, flags);
        } else {
            if (connect(fd, rp->ai_addr, rp->ai_addrlen) < 0) {
                close(fd);
                fd = -1;
                continue;
            }
        }
        break; /* Connected */
    }

    freeaddrinfo(result);
    return fd >= 0 ? fd : WL_INVALID_SOCKET;
}

void wl_platform_socket_close(wl_socket_t sock) {
    if (sock >= 0)
        close(sock);
}

int wl_platform_socket_send(wl_socket_t sock, const void *data, size_t len) {
    const char *p = data;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(sock, p + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        sent += (size_t)n;
    }
    return (int)sent;
}

int wl_platform_socket_recv(wl_socket_t sock, void *buf, size_t len) {
    ssize_t n;
    do {
        n = recv(sock, buf, len, 0);
    } while (n < 0 && errno == EINTR);
    return (int)n;
}

int wl_platform_socket_set_timeout(wl_socket_t sock, uint32_t timeout_ms) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        return -1;
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0)
        return -1;
    return 0;
}

/* ------- DNS ------- */

int wl_platform_resolve(const char *host, char *addr_str, size_t addr_str_len) {
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(host, NULL, &hints, &result);
    if (rc != 0)
        return -1;

    void *addr;
    if (result->ai_family == AF_INET) {
        addr = &((struct sockaddr_in *)result->ai_addr)->sin_addr;
    } else {
        addr = &((struct sockaddr_in6 *)result->ai_addr)->sin6_addr;
    }

    if (!inet_ntop(result->ai_family, addr, addr_str, (socklen_t)addr_str_len)) {
        freeaddrinfo(result);
        return -1;
    }

    freeaddrinfo(result);
    return 0;
}

/* ------- Random ------- */

int wl_platform_random(void *buf, size_t len) {
    /* Try arc4random_buf first (available on macOS, modern BSD, glibc 2.36+) */
#if defined(__APPLE__) || defined(__OpenBSD__) || defined(__FreeBSD__)
    arc4random_buf(buf, len);
    return 0;
#else
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f)
        return -1;
    size_t n = fread(buf, 1, len, f);
    fclose(f);
    return n == len ? 0 : -1;
#endif
}

/* ------- Time ------- */

uint64_t wl_platform_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/* ------- Threading ------- */

#ifdef WEBLITE_HAS_THREADS

wl_thread_t wl_platform_thread_create(wl_thread_func_t func, void *arg) {
    pthread_t *t = malloc(sizeof(pthread_t));
    if (!t) return NULL;
    if (pthread_create(t, NULL, func, arg) != 0) {
        free(t);
        return NULL;
    }
    return t;
}

int wl_platform_thread_join(wl_thread_t thread) {
    pthread_t *t = thread;
    int rc = pthread_join(*t, NULL);
    free(t);
    return rc;
}

wl_mutex_t wl_platform_mutex_create(void) {
    pthread_mutex_t *m = malloc(sizeof(pthread_mutex_t));
    if (!m) return NULL;
    if (pthread_mutex_init(m, NULL) != 0) {
        free(m);
        return NULL;
    }
    return m;
}

void wl_platform_mutex_destroy(wl_mutex_t mutex) {
    if (mutex) {
        pthread_mutex_destroy(mutex);
        free(mutex);
    }
}

void wl_platform_mutex_lock(wl_mutex_t mutex) {
    pthread_mutex_lock(mutex);
}

void wl_platform_mutex_unlock(wl_mutex_t mutex) {
    pthread_mutex_unlock(mutex);
}

wl_cond_t wl_platform_cond_create(void) {
    pthread_cond_t *c = malloc(sizeof(pthread_cond_t));
    if (!c) return NULL;
    if (pthread_cond_init(c, NULL) != 0) {
        free(c);
        return NULL;
    }
    return c;
}

void wl_platform_cond_destroy(wl_cond_t cond) {
    if (cond) {
        pthread_cond_destroy(cond);
        free(cond);
    }
}

void wl_platform_cond_wait(wl_cond_t cond, wl_mutex_t mutex) {
    pthread_cond_wait(cond, mutex);
}

int wl_platform_cond_timedwait(wl_cond_t cond, wl_mutex_t mutex, uint32_t timeout_ms) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (long)(timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
    return pthread_cond_timedwait(cond, mutex, &ts);
}

void wl_platform_cond_signal(wl_cond_t cond) {
    pthread_cond_signal(cond);
}

void wl_platform_cond_broadcast(wl_cond_t cond) {
    pthread_cond_broadcast(cond);
}

#endif /* WEBLITE_HAS_THREADS */

#endif /* WEBLITE_POSIX */

/* --- platform_win32.c --- */

#ifdef WEBLITE_WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <bcrypt.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")

/* ------- Init / Cleanup ------- */

int wl_platform_init(void) {
    WSADATA wsa_data;
    return WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0 ? 0 : -1;
}

void wl_platform_cleanup(void) {
    WSACleanup();
}

/* ------- Socket ------- */

wl_socket_t wl_platform_socket_open(const char *host, uint16_t port, uint32_t timeout_ms) {
    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    _snprintf_s(port_str, sizeof(port_str), _TRUNCATE, "%u", port);

    if (getaddrinfo(host, port_str, &hints, &result) != 0)
        return WL_INVALID_SOCKET;

    SOCKET fd = INVALID_SOCKET;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == INVALID_SOCKET)
            continue;

        if (timeout_ms > 0) {
            u_long nonblock = 1;
            ioctlsocket(fd, FIONBIO, &nonblock);

            int rc = connect(fd, rp->ai_addr, (int)rp->ai_addrlen);
            if (rc == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) {
                fd_set wset;
                FD_ZERO(&wset);
                FD_SET(fd, &wset);
                struct timeval tv;
                tv.tv_sec = timeout_ms / 1000;
                tv.tv_usec = (timeout_ms % 1000) * 1000;

                if (select(0, NULL, &wset, NULL, &tv) <= 0) {
                    closesocket(fd);
                    fd = INVALID_SOCKET;
                    continue;
                }

                int err = 0;
                int errlen = sizeof(err);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&err, &errlen);
                if (err != 0) {
                    closesocket(fd);
                    fd = INVALID_SOCKET;
                    continue;
                }
            } else if (rc == SOCKET_ERROR) {
                closesocket(fd);
                fd = INVALID_SOCKET;
                continue;
            }

            nonblock = 0;
            ioctlsocket(fd, FIONBIO, &nonblock);
        } else {
            if (connect(fd, rp->ai_addr, (int)rp->ai_addrlen) == SOCKET_ERROR) {
                closesocket(fd);
                fd = INVALID_SOCKET;
                continue;
            }
        }
        break;
    }

    freeaddrinfo(result);

    if (fd == INVALID_SOCKET)
        return WL_INVALID_SOCKET;

    /* Windows SOCKET is unsigned, cast to int (wl_socket_t) */
    return (wl_socket_t)fd;
}

void wl_platform_socket_close(wl_socket_t sock) {
    if (sock >= 0)
        closesocket((SOCKET)sock);
}

int wl_platform_socket_send(wl_socket_t sock, const void *data, size_t len) {
    const char *p = data;
    size_t sent = 0;
    while (sent < len) {
        int n = send((SOCKET)sock, p + sent, (int)(len - sent), 0);
        if (n == SOCKET_ERROR)
            return -1;
        sent += (size_t)n;
    }
    return (int)sent;
}

int wl_platform_socket_recv(wl_socket_t sock, void *buf, size_t len) {
    return recv((SOCKET)sock, buf, (int)len, 0);
}

int wl_platform_socket_set_timeout(wl_socket_t sock, uint32_t timeout_ms) {
    DWORD tv = timeout_ms;
    if (setsockopt((SOCKET)sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)) != 0)
        return -1;
    if (setsockopt((SOCKET)sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv)) != 0)
        return -1;
    return 0;
}

/* ------- DNS ------- */

int wl_platform_resolve(const char *host, char *addr_str, size_t addr_str_len) {
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, NULL, &hints, &result) != 0)
        return -1;

    DWORD len = (DWORD)addr_str_len;
    int rc = WSAAddressToStringA(result->ai_addr, (DWORD)result->ai_addrlen,
                                  NULL, addr_str, &len);
    freeaddrinfo(result);
    return rc == 0 ? 0 : -1;
}

/* ------- Random ------- */

int wl_platform_random(void *buf, size_t len) {
    NTSTATUS status = BCryptGenRandom(NULL, buf, (ULONG)len,
                                       BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return status == 0 ? 0 : -1;
}

/* ------- Time ------- */

uint64_t wl_platform_time_ms(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)(count.QuadPart * 1000 / freq.QuadPart);
}

/* ------- Threading ------- */

#ifdef WEBLITE_HAS_THREADS

typedef struct {
    wl_thread_func_t func;
    void *arg;
} win32_thread_arg_t;

static DWORD WINAPI win32_thread_entry(LPVOID param) {
    win32_thread_arg_t *ta = param;
    wl_thread_func_t func = ta->func;
    void *arg = ta->arg;
    free(ta);
    func(arg);
    return 0;
}

wl_thread_t wl_platform_thread_create(wl_thread_func_t func, void *arg) {
    win32_thread_arg_t *ta = malloc(sizeof(win32_thread_arg_t));
    if (!ta) return NULL;
    ta->func = func;
    ta->arg = arg;

    HANDLE *h = malloc(sizeof(HANDLE));
    if (!h) { free(ta); return NULL; }

    *h = CreateThread(NULL, 0, win32_thread_entry, ta, 0, NULL);
    if (*h == NULL) { free(ta); free(h); return NULL; }
    return h;
}

int wl_platform_thread_join(wl_thread_t thread) {
    HANDLE *h = thread;
    WaitForSingleObject(*h, INFINITE);
    CloseHandle(*h);
    free(h);
    return 0;
}

wl_mutex_t wl_platform_mutex_create(void) {
    CRITICAL_SECTION *cs = malloc(sizeof(CRITICAL_SECTION));
    if (!cs) return NULL;
    InitializeCriticalSection(cs);
    return cs;
}

void wl_platform_mutex_destroy(wl_mutex_t mutex) {
    if (mutex) {
        DeleteCriticalSection(mutex);
        free(mutex);
    }
}

void wl_platform_mutex_lock(wl_mutex_t mutex) {
    EnterCriticalSection(mutex);
}

void wl_platform_mutex_unlock(wl_mutex_t mutex) {
    LeaveCriticalSection(mutex);
}

wl_cond_t wl_platform_cond_create(void) {
    CONDITION_VARIABLE *cv = malloc(sizeof(CONDITION_VARIABLE));
    if (!cv) return NULL;
    InitializeConditionVariable(cv);
    return cv;
}

void wl_platform_cond_destroy(wl_cond_t cond) {
    free(cond); /* No DestroyConditionVariable on Windows */
}

void wl_platform_cond_wait(wl_cond_t cond, wl_mutex_t mutex) {
    SleepConditionVariableCS(cond, mutex, INFINITE);
}

int wl_platform_cond_timedwait(wl_cond_t cond, wl_mutex_t mutex, uint32_t timeout_ms) {
    if (!SleepConditionVariableCS(cond, mutex, timeout_ms))
        return -1; /* Timeout */
    return 0;
}

void wl_platform_cond_signal(wl_cond_t cond) {
    WakeConditionVariable(cond);
}

void wl_platform_cond_broadcast(wl_cond_t cond) {
    WakeAllConditionVariable(cond);
}

#endif /* WEBLITE_HAS_THREADS */

#endif /* WEBLITE_WIN32 */

/* --- platform_wasm.c --- */

#ifdef WEBLITE_WASM

#include <emscripten.h>
#include <emscripten/fetch.h>
#include <emscripten/websocket.h>
#include <time.h>

/*
 * WASM/Emscripten platform
 *
 * HTTP requests use emscripten_fetch() which maps to browser fetch().
 * WebSocket uses emscripten_websocket_* which maps to browser WebSocket.
 * TLS is handled transparently by the browser.
 * No raw sockets are needed.
 */

int wl_platform_init(void) {
    return 0;
}

void wl_platform_cleanup(void) {
}

/*
 * Socket stubs - not used on WASM.
 * HTTP and WS go through higher-level browser APIs.
 */

wl_socket_t wl_platform_socket_open(const char *host, uint16_t port, uint32_t timeout_ms) {
    (void)host; (void)port; (void)timeout_ms;
    return WL_INVALID_SOCKET;
}

void wl_platform_socket_close(wl_socket_t sock) {
    (void)sock;
}

int wl_platform_socket_send(wl_socket_t sock, const void *data, size_t len) {
    (void)sock; (void)data; (void)len;
    return -1;
}

int wl_platform_socket_recv(wl_socket_t sock, void *buf, size_t len) {
    (void)sock; (void)buf; (void)len;
    return -1;
}

int wl_platform_socket_set_timeout(wl_socket_t sock, uint32_t timeout_ms) {
    (void)sock; (void)timeout_ms;
    return -1;
}

int wl_platform_resolve(const char *host, char *addr_str, size_t addr_str_len) {
    (void)host; (void)addr_str; (void)addr_str_len;
    return -1; /* DNS handled by browser */
}

int wl_platform_random(void *buf, size_t len) {
    /* Use JavaScript crypto.getRandomValues via EM_ASM */
    EM_ASM({
        var arr = new Uint8Array(HEAPU8.buffer, $0, $1);
        crypto.getRandomValues(arr);
    }, buf, (int)len);
    return 0;
}

uint64_t wl_platform_time_ms(void) {
    return (uint64_t)emscripten_get_now();
}

/* Threading not supported on WASM (unless using SharedArrayBuffer + pthreads) */

#endif /* WEBLITE_WASM */

/* --- platform_embedded.c --- */

#ifdef WEBLITE_EMBEDDED


/*
 * Embedded platform - uses user-provided hooks via weblite_set_platform_hooks().
 * All socket/random operations dispatch through function pointers.
 */

static weblite_platform_hooks_t g_hooks;
static int g_hooks_set = 0;

/* Called from weblite_set_platform_hooks() */
void wl_platform_embedded_set_hooks(const weblite_platform_hooks_t *hooks) {
    if (hooks) {
        g_hooks = *hooks;
        g_hooks_set = 1;
    }
}

int wl_platform_init(void) {
    return 0;
}

void wl_platform_cleanup(void) {
    g_hooks_set = 0;
    memset(&g_hooks, 0, sizeof(g_hooks));
}

wl_socket_t wl_platform_socket_open(const char *host, uint16_t port, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (!g_hooks_set || !g_hooks.socket_open)
        return WL_INVALID_SOCKET;
    return g_hooks.socket_open(host, port);
}

void wl_platform_socket_close(wl_socket_t sock) {
    if (g_hooks_set && g_hooks.socket_close)
        g_hooks.socket_close(sock);
}

int wl_platform_socket_send(wl_socket_t sock, const void *data, size_t len) {
    if (!g_hooks_set || !g_hooks.socket_send)
        return -1;
    return g_hooks.socket_send(sock, data, len);
}

int wl_platform_socket_recv(wl_socket_t sock, void *buf, size_t len) {
    if (!g_hooks_set || !g_hooks.socket_recv)
        return -1;
    return g_hooks.socket_recv(sock, buf, len);
}

int wl_platform_socket_set_timeout(wl_socket_t sock, uint32_t timeout_ms) {
    (void)sock; (void)timeout_ms;
    return 0; /* No-op for embedded; user controls timeouts in hooks */
}

int wl_platform_resolve(const char *host, char *addr_str, size_t addr_str_len) {
    (void)host; (void)addr_str; (void)addr_str_len;
    return -1; /* User's socket_open handles DNS */
}

int wl_platform_random(void *buf, size_t len) {
    if (!g_hooks_set || !g_hooks.random_bytes)
        return -1;
    return g_hooks.random_bytes(buf, len);
}

uint64_t wl_platform_time_ms(void) {
    return 0; /* User should provide a timer if needed */
}

/* Threading stubs - embedded platforms typically don't use the thread pool */

#endif /* WEBLITE_EMBEDDED */

/* --- weblite.c --- */

static weblite_config_t g_config;
static int g_initialized = 0;

weblite_config_t weblite_config_defaults(void) {
    weblite_config_t config;
    memset(&config, 0, sizeof(config));
    config.default_transport = WEBLITE_TRANSPORT_AUTO;
    config.default_timeout_ms = 30000;
    config.default_max_redirects = 10;
    config.tls_verify = 1;
    config.ca_path = NULL;
    config.thread_pool_size = 4;
    config.allocator = NULL;
    return config;
}

weblite_error_t weblite_init(const weblite_config_t *config) {
    if (g_initialized)
        return WEBLITE_OK;

    if (config)
        g_config = *config;
    else
        g_config = weblite_config_defaults();

    if (wl_platform_init() != 0)
        return WEBLITE_ERR_NOT_INITIALIZED;

#ifdef WEBLITE_HAS_TLS
    if (wl_tls_init() != 0) {
        wl_platform_cleanup();
        return WEBLITE_ERR_TLS;
    }
#endif

#ifdef WEBLITE_HAS_THREADS
    if (wl_async_init(g_config.thread_pool_size) != 0) {
        wl_tls_cleanup();
        wl_platform_cleanup();
        return WEBLITE_ERR_NOT_INITIALIZED;
    }
#endif

    g_initialized = 1;
    return WEBLITE_OK;
}

void weblite_cleanup(void) {
    if (!g_initialized)
        return;

#ifdef WEBLITE_HAS_THREADS
    wl_async_cleanup();
#endif

#ifdef WEBLITE_HAS_TLS
    wl_tls_cleanup();
#endif

    wl_platform_cleanup();
    g_initialized = 0;
}

/* Synchronous request */
weblite_error_t weblite_request(const weblite_request_t *req,
                                weblite_response_t **resp) {
    if (!g_initialized)
        return WEBLITE_ERR_NOT_INITIALIZED;
    if (!req || !req->url || !resp)
        return WEBLITE_ERR_INVALID_ARG;

    *resp = calloc(1, sizeof(weblite_response_t));
    if (!*resp)
        return WEBLITE_ERR_ALLOC;

    weblite_transport_mode_t mode = req->transport_mode;
    if (mode == WEBLITE_TRANSPORT_AUTO)
        mode = g_config.default_transport;

    uint32_t timeout = req->timeout_ms > 0 ? req->timeout_ms : g_config.default_timeout_ms;
    uint32_t redirects = req->max_redirects > 0 ? req->max_redirects : g_config.default_max_redirects;

    weblite_error_t err = wl_transport_execute(req, *resp, mode, timeout,
                                                redirects, g_config.tls_verify,
                                                g_config.ca_path);
    (*resp)->error = err;
    return err;
}

weblite_error_t weblite_get(const char *url,
                            const weblite_header_t *headers, size_t header_count,
                            weblite_response_t **resp) {
    weblite_request_t req;
    memset(&req, 0, sizeof(req));
    req.url = url;
    req.method = WEBLITE_METHOD_GET;
    req.headers = headers;
    req.header_count = header_count;
    return weblite_request(&req, resp);
}

weblite_error_t weblite_post(const char *url,
                             const weblite_header_t *headers, size_t header_count,
                             const void *body, size_t body_len,
                             weblite_response_t **resp) {
    weblite_request_t req;
    memset(&req, 0, sizeof(req));
    req.url = url;
    req.method = WEBLITE_METHOD_POST;
    req.headers = headers;
    req.header_count = header_count;
    req.body = body;
    req.body_len = body_len;
    return weblite_request(&req, resp);
}

weblite_error_t weblite_put(const char *url,
                            const weblite_header_t *headers, size_t header_count,
                            const void *body, size_t body_len,
                            weblite_response_t **resp) {
    weblite_request_t req;
    memset(&req, 0, sizeof(req));
    req.url = url;
    req.method = WEBLITE_METHOD_PUT;
    req.headers = headers;
    req.header_count = header_count;
    req.body = body;
    req.body_len = body_len;
    return weblite_request(&req, resp);
}

weblite_error_t weblite_delete(const char *url,
                               const weblite_header_t *headers, size_t header_count,
                               weblite_response_t **resp) {
    weblite_request_t req;
    memset(&req, 0, sizeof(req));
    req.url = url;
    req.method = WEBLITE_METHOD_DELETE;
    req.headers = headers;
    req.header_count = header_count;
    return weblite_request(&req, resp);
}

/* Async wrappers */
weblite_async_handle_t *weblite_request_async(const weblite_request_t *req,
                                               weblite_callback_t callback,
                                               void *user_data) {
#ifdef WEBLITE_HAS_THREADS
    if (!g_initialized || !req || !req->url)
        return NULL;
    return wl_async_submit(req, callback, user_data, &g_config);
#else
    (void)req; (void)callback; (void)user_data;
    return NULL;
#endif
}

weblite_async_handle_t *weblite_get_async(const char *url,
                                           const weblite_header_t *headers,
                                           size_t header_count,
                                           weblite_callback_t callback,
                                           void *user_data) {
    weblite_request_t req;
    memset(&req, 0, sizeof(req));
    req.url = url;
    req.method = WEBLITE_METHOD_GET;
    req.headers = headers;
    req.header_count = header_count;
    req.user_data = user_data;
    return weblite_request_async(&req, callback, user_data);
}

weblite_async_handle_t *weblite_post_async(const char *url,
                                            const weblite_header_t *headers,
                                            size_t header_count,
                                            const void *body, size_t body_len,
                                            weblite_callback_t callback,
                                            void *user_data) {
    weblite_request_t req;
    memset(&req, 0, sizeof(req));
    req.url = url;
    req.method = WEBLITE_METHOD_POST;
    req.headers = headers;
    req.header_count = header_count;
    req.body = body;
    req.body_len = body_len;
    req.user_data = user_data;
    return weblite_request_async(&req, callback, user_data);
}

weblite_async_handle_t *weblite_put_async(const char *url,
                                           const weblite_header_t *headers,
                                           size_t header_count,
                                           const void *body, size_t body_len,
                                           weblite_callback_t callback,
                                           void *user_data) {
    weblite_request_t req;
    memset(&req, 0, sizeof(req));
    req.url = url;
    req.method = WEBLITE_METHOD_PUT;
    req.headers = headers;
    req.header_count = header_count;
    req.body = body;
    req.body_len = body_len;
    req.user_data = user_data;
    return weblite_request_async(&req, callback, user_data);
}

weblite_async_handle_t *weblite_delete_async(const char *url,
                                              const weblite_header_t *headers,
                                              size_t header_count,
                                              weblite_callback_t callback,
                                              void *user_data) {
    weblite_request_t req;
    memset(&req, 0, sizeof(req));
    req.url = url;
    req.method = WEBLITE_METHOD_DELETE;
    req.headers = headers;
    req.header_count = header_count;
    req.user_data = user_data;
    return weblite_request_async(&req, callback, user_data);
}

weblite_error_t weblite_async_cancel(weblite_async_handle_t *handle) {
#ifdef WEBLITE_HAS_THREADS
    return wl_async_cancel(handle);
#else
    (void)handle;
    return WEBLITE_ERR_UNSUPPORTED;
#endif
}

weblite_error_t weblite_async_wait(weblite_async_handle_t *handle,
                                   uint32_t timeout_ms) {
#ifdef WEBLITE_HAS_THREADS
    return wl_async_wait(handle, timeout_ms);
#else
    (void)handle; (void)timeout_ms;
    return WEBLITE_ERR_UNSUPPORTED;
#endif
}

int weblite_async_poll(void) {
#ifdef WEBLITE_HAS_THREADS
    return wl_async_poll();
#else
    return 0;
#endif
}

void weblite_response_free(weblite_response_t *resp) {
    if (!resp) return;
    for (size_t i = 0; i < resp->header_count; i++) {
        free(resp->headers[i].name);
        free(resp->headers[i].value);
    }
    free(resp->headers);
    free(resp->body);
    free(resp);
}

const char *weblite_error_string(weblite_error_t err) {
    switch (err) {
    case WEBLITE_OK:                    return "OK";
    case WEBLITE_ERR_INVALID_ARG:       return "Invalid argument";
    case WEBLITE_ERR_ALLOC:             return "Memory allocation failed";
    case WEBLITE_ERR_DNS:               return "DNS resolution failed";
    case WEBLITE_ERR_CONNECT:           return "Connection failed";
    case WEBLITE_ERR_TLS:               return "TLS error";
    case WEBLITE_ERR_TIMEOUT:           return "Request timed out";
    case WEBLITE_ERR_HTTP_PARSE:        return "HTTP parse error";
    case WEBLITE_ERR_WS_HANDSHAKE:      return "WebSocket handshake failed";
    case WEBLITE_ERR_WS_PROTOCOL:       return "WebSocket protocol error";
    case WEBLITE_ERR_TRANSPORT:         return "Transport error";
    case WEBLITE_ERR_CANCELLED:         return "Request cancelled";
    case WEBLITE_ERR_NOT_INITIALIZED:   return "Library not initialized";
    case WEBLITE_ERR_IO:                return "I/O error";
    case WEBLITE_ERR_REDIRECT_LIMIT:    return "Too many redirects";
    case WEBLITE_ERR_UNSUPPORTED:       return "Unsupported operation";
    }
    return "Unknown error";
}

const char *weblite_version(void) {
    return WEBLITE_VERSION_STRING;
}

weblite_error_t weblite_set_platform_hooks(const weblite_platform_hooks_t *hooks) {
    (void)hooks;
    /* Will be implemented by platform_embedded.c */
    return WEBLITE_OK;
}

