/*
 * weblite.h - Amalgamated public header
 *
 * Embeddable C REST API client with HTTP/WebSocket transport.
 * This is a single-file header generated from the weblite source tree.
 *
 * Include this file in your project, then compile weblite.c alongside
 * your application code.
 */
#ifndef WEBLITE_H
#define WEBLITE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- weblite_config.h --- */

#define WEBLITE_VERSION_MAJOR 1
#define WEBLITE_VERSION_MINOR 0
#define WEBLITE_VERSION_PATCH 0
#define WEBLITE_VERSION_STRING "1.0.0"

#define WEBLITE_ENABLE_TLS
#define WEBLITE_ENABLE_THREADS

#define WEBLITE_PLATFORM_POSIX
/* #undef WEBLITE_PLATFORM_WIN32 */
/* #undef WEBLITE_PLATFORM_WASM */
/* #undef WEBLITE_PLATFORM_EMBEDDED */


/* --- weblite_types.h --- */



/* Error codes */
typedef enum {
    WEBLITE_OK = 0,
    WEBLITE_ERR_INVALID_ARG,
    WEBLITE_ERR_ALLOC,
    WEBLITE_ERR_DNS,
    WEBLITE_ERR_CONNECT,
    WEBLITE_ERR_TLS,
    WEBLITE_ERR_TIMEOUT,
    WEBLITE_ERR_HTTP_PARSE,
    WEBLITE_ERR_WS_HANDSHAKE,
    WEBLITE_ERR_WS_PROTOCOL,
    WEBLITE_ERR_TRANSPORT,
    WEBLITE_ERR_CANCELLED,
    WEBLITE_ERR_NOT_INITIALIZED,
    WEBLITE_ERR_IO,
    WEBLITE_ERR_REDIRECT_LIMIT,
    WEBLITE_ERR_UNSUPPORTED
} weblite_error_t;

/* HTTP methods */
typedef enum {
    WEBLITE_METHOD_GET = 0,
    WEBLITE_METHOD_POST,
    WEBLITE_METHOD_PUT,
    WEBLITE_METHOD_DELETE,
    WEBLITE_METHOD_PATCH,
    WEBLITE_METHOD_HEAD,
    WEBLITE_METHOD_OPTIONS
} weblite_method_t;

/* Transport selection */
typedef enum {
    WEBLITE_TRANSPORT_AUTO = 0,
    WEBLITE_TRANSPORT_HTTP_ONLY,
    WEBLITE_TRANSPORT_WS_ONLY
} weblite_transport_mode_t;

/* Which transport was actually used */
typedef enum {
    WEBLITE_USED_HTTP = 0,
    WEBLITE_USED_WS
} weblite_transport_used_t;

/* Key-value header pair */
typedef struct {
    const char *name;
    const char *value;
} weblite_header_t;

/* Owned header (for response) */
typedef struct {
    char *name;
    char *value;
} weblite_header_owned_t;

/* Request configuration */
typedef struct {
    const char              *url;
    weblite_method_t         method;
    const weblite_header_t  *headers;
    size_t                   header_count;
    const void              *body;
    size_t                   body_len;
    weblite_transport_mode_t transport_mode;
    uint32_t                 timeout_ms;
    uint32_t                 max_redirects;
    void                    *user_data;
} weblite_request_t;

/* Response */
typedef struct {
    int                       status_code;
    weblite_header_owned_t   *headers;
    size_t                    header_count;
    void                     *body;
    size_t                    body_len;
    weblite_transport_used_t  transport_used;
    weblite_error_t           error;
    char                      error_msg[256];
} weblite_response_t;

/* Custom allocator */
typedef struct {
    void *(*malloc)(size_t size);
    void *(*realloc)(void *ptr, size_t size);
    void  (*free)(void *ptr);
} weblite_allocator_t;

/* Global configuration */
typedef struct {
    weblite_transport_mode_t  default_transport;
    uint32_t                  default_timeout_ms;
    uint32_t                  default_max_redirects;
    int                       tls_verify;
    const char               *ca_path;
    int                       thread_pool_size;
    weblite_allocator_t      *allocator;
} weblite_config_t;

/* Async callback */
typedef void (*weblite_callback_t)(const weblite_response_t *resp, void *user_data);

/* Opaque async handle */
typedef struct weblite_async_handle weblite_async_handle_t;

/* Platform hooks for embedded targets */
typedef struct {
    int   (*socket_open)(const char *host, uint16_t port);
    void  (*socket_close)(int fd);
    int   (*socket_send)(int fd, const void *data, size_t len);
    int   (*socket_recv)(int fd, void *buf, size_t len);
    int   (*random_bytes)(void *buf, size_t len);
    void *user_data;
} weblite_platform_hooks_t;



/* --- weblite.h API --- */



/* Lifecycle */
weblite_error_t   weblite_init(const weblite_config_t *config);
void              weblite_cleanup(void);
weblite_config_t  weblite_config_defaults(void);

/* Synchronous requests */
weblite_error_t weblite_request(const weblite_request_t *req,
                                weblite_response_t **resp);

weblite_error_t weblite_get(const char *url,
                            const weblite_header_t *headers, size_t header_count,
                            weblite_response_t **resp);

weblite_error_t weblite_post(const char *url,
                             const weblite_header_t *headers, size_t header_count,
                             const void *body, size_t body_len,
                             weblite_response_t **resp);

weblite_error_t weblite_put(const char *url,
                            const weblite_header_t *headers, size_t header_count,
                            const void *body, size_t body_len,
                            weblite_response_t **resp);

weblite_error_t weblite_delete(const char *url,
                               const weblite_header_t *headers, size_t header_count,
                               weblite_response_t **resp);

/* Asynchronous requests */
weblite_async_handle_t *weblite_request_async(const weblite_request_t *req,
                                               weblite_callback_t callback,
                                               void *user_data);

weblite_async_handle_t *weblite_get_async(const char *url,
                                           const weblite_header_t *headers,
                                           size_t header_count,
                                           weblite_callback_t callback,
                                           void *user_data);

weblite_async_handle_t *weblite_post_async(const char *url,
                                            const weblite_header_t *headers,
                                            size_t header_count,
                                            const void *body, size_t body_len,
                                            weblite_callback_t callback,
                                            void *user_data);

weblite_async_handle_t *weblite_put_async(const char *url,
                                           const weblite_header_t *headers,
                                           size_t header_count,
                                           const void *body, size_t body_len,
                                           weblite_callback_t callback,
                                           void *user_data);

weblite_async_handle_t *weblite_delete_async(const char *url,
                                              const weblite_header_t *headers,
                                              size_t header_count,
                                              weblite_callback_t callback,
                                              void *user_data);

weblite_error_t weblite_async_cancel(weblite_async_handle_t *handle);
weblite_error_t weblite_async_wait(weblite_async_handle_t *handle,
                                   uint32_t timeout_ms);
int             weblite_async_poll(void);

/* Utility */
void        weblite_response_free(weblite_response_t *resp);
const char *weblite_error_string(weblite_error_t err);
const char *weblite_version(void);

/* Platform hooks (for embedded targets) */
weblite_error_t weblite_set_platform_hooks(const weblite_platform_hooks_t *hooks);



#ifdef __cplusplus
}
#endif

#endif /* WEBLITE_H */
