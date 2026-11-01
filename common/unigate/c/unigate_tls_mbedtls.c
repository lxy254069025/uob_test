#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

typedef int wl_socket_t;

extern int wl_platform_socket_send(wl_socket_t socket, const void *data, size_t len);
extern int wl_platform_socket_recv(wl_socket_t socket, void *buf, size_t len);

typedef struct wl_tls {
    wl_socket_t socket;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config config;
    mbedtls_x509_crt ca_chain;
    mbedtls_ctr_drbg_context random;
    mbedtls_entropy_context entropy;
} wl_tls_t;

static _Thread_local const char *request_ca_path;
static _Thread_local char tls_error[256];

void unigate_tls_set_request_ca_path(const char *path) {
    request_ca_path = path;
}

const char *wl_tls_last_error(void) {
    return tls_error[0] == '\0' ? NULL : tls_error;
}

static void set_tls_error(const char *stage, int result) {
    snprintf(tls_error, sizeof(tls_error), "%s: mbedTLS error %d (0x%04X)",
             stage, result, (unsigned int)result & 0xFFFFU);
}

static void set_tls_io_error(const char *stage, const mbedtls_ssl_context *ssl, int result) {
    if (ssl != NULL &&
        ssl->MBEDTLS_PRIVATE(in_msgtype) == MBEDTLS_SSL_MSG_ALERT &&
        ssl->MBEDTLS_PRIVATE(in_msglen) >= 2 &&
        ssl->MBEDTLS_PRIVATE(in_msg) != NULL) {
        snprintf(tls_error, sizeof(tls_error),
                 "%s: TLS alert level=%u description=%u; mbedTLS error %d (0x%04X)",
                 stage,
                 ssl->MBEDTLS_PRIVATE(in_msg)[0],
                 ssl->MBEDTLS_PRIVATE(in_msg)[1],
                 result,
                 (unsigned int)result & 0xFFFFU);
        return;
    }
    set_tls_error(stage, result);
}

static int tls_is_close_notify(const mbedtls_ssl_context *ssl) {
    return ssl != NULL &&
           ssl->MBEDTLS_PRIVATE(in_msgtype) == MBEDTLS_SSL_MSG_ALERT &&
           ssl->MBEDTLS_PRIVATE(in_msglen) >= 2 &&
           ssl->MBEDTLS_PRIVATE(in_msg) != NULL &&
           ssl->MBEDTLS_PRIVATE(in_msg)[1] == MBEDTLS_SSL_ALERT_MSG_CLOSE_NOTIFY;
}

static void set_tls_verify_error(int flags) {
    char detail[384];

    mbedtls_x509_crt_verify_info(detail, sizeof(detail), "! ", flags);
    snprintf(tls_error, sizeof(tls_error),
             "certificate verification failed: %.200s", detail);
}

static int tls_transport_send(void *context, const unsigned char *buf, size_t len) {
    wl_tls_t *tls = context;
    int result = wl_platform_socket_send(tls->socket, buf, len);

    if (result < 0) {
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    }
    return result;
}

static int tls_transport_recv(void *context, unsigned char *buf, size_t len) {
    wl_tls_t *tls = context;
    int result = wl_platform_socket_recv(tls->socket, buf, len);

    if (result < 0) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    return result;
}

static void tls_destroy(wl_tls_t *tls) {
    if (tls == NULL) {
        return;
    }
    mbedtls_ssl_free(&tls->ssl);
    mbedtls_ssl_config_free(&tls->config);
    mbedtls_x509_crt_free(&tls->ca_chain);
    mbedtls_ctr_drbg_free(&tls->random);
    mbedtls_entropy_free(&tls->entropy);
    free(tls);
}

int wl_tls_init(void) {
    return 0;
}

void wl_tls_cleanup(void) {
}

wl_tls_t *wl_tls_connect(
    wl_socket_t socket,
    const char *hostname,
    int verify,
    const char *ca_path
) {
    static const unsigned char personalization[] = "uopenbox-unigate";
    const char *effective_ca_path = request_ca_path != NULL ? request_ca_path : ca_path;
    wl_tls_t *tls = calloc(1, sizeof(*tls));
    int result;

    tls_error[0] = '\0';
    if (tls == NULL || hostname == NULL) {
        free(tls);
        snprintf(tls_error, sizeof(tls_error), "initializing TLS context failed");
        return NULL;
    }
    tls->socket = socket;
    mbedtls_ssl_init(&tls->ssl);
    mbedtls_ssl_config_init(&tls->config);
    mbedtls_x509_crt_init(&tls->ca_chain);
    mbedtls_ctr_drbg_init(&tls->random);
    mbedtls_entropy_init(&tls->entropy);

    result = mbedtls_ctr_drbg_seed(
        &tls->random,
        mbedtls_entropy_func,
        &tls->entropy,
        personalization,
        sizeof(personalization) - 1
    );
    if (result != 0) {
        set_tls_error("initializing random generator", result);
        tls_destroy(tls);
        return NULL;
    }
    result = mbedtls_ssl_config_defaults(
        &tls->config,
        MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT
    );
    if (result != 0) {
        set_tls_error("initializing TLS configuration", result);
        tls_destroy(tls);
        return NULL;
    }

    mbedtls_ssl_conf_rng(&tls->config, mbedtls_ctr_drbg_random, &tls->random);
    mbedtls_ssl_conf_min_tls_version(&tls->config, MBEDTLS_SSL_VERSION_TLS1_2);

    if (verify) {
        if (effective_ca_path == NULL || effective_ca_path[0] == '\0') {
            snprintf(tls_error, sizeof(tls_error),
                     "certificate verification requires a CA bundle; set ca_path or UNIGATE_CA_BUNDLE");
            tls_destroy(tls);
            return NULL;
        }
        result = mbedtls_x509_crt_parse_file(&tls->ca_chain, effective_ca_path);
        if (result < 0 || tls->ca_chain.raw.p == NULL) {
            set_tls_error("parsing CA bundle", result);
            tls_destroy(tls);
            return NULL;
        }
        mbedtls_ssl_conf_authmode(&tls->config, MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_ca_chain(&tls->config, &tls->ca_chain, NULL);
    } else {
        mbedtls_ssl_conf_authmode(&tls->config, MBEDTLS_SSL_VERIFY_NONE);
    }

    result = mbedtls_ssl_setup(&tls->ssl, &tls->config);
    if (result == 0) {
        result = mbedtls_ssl_set_hostname(&tls->ssl, hostname);
    }
    if (result != 0) {
        set_tls_error("initializing TLS session", result);
        tls_destroy(tls);
        return NULL;
    }
    mbedtls_ssl_set_bio(
        &tls->ssl,
        tls,
        tls_transport_send,
        tls_transport_recv,
        NULL
    );

    do {
        result = mbedtls_ssl_handshake(&tls->ssl);
    } while (result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE);

    if (result != 0) {
        set_tls_error("TLS handshake", result);
        tls_destroy(tls);
        return NULL;
    }
    if (verify) {
        uint32_t flags = mbedtls_ssl_get_verify_result(&tls->ssl);
        if (flags != 0) {
            set_tls_verify_error((int)flags);
            tls_destroy(tls);
            return NULL;
        }
    }
    return tls;
}

int wl_tls_send(wl_tls_t *tls, const void *data, size_t len) {
    const unsigned char *cur = data;
    size_t written = 0;

    if (tls == NULL) {
        return -1;
    }
    while (written < len) {
        int result = mbedtls_ssl_write(&tls->ssl, cur + written, len - written);
        if (result > 0) {
            written += (size_t)result;
            continue;
        }
        if (result != MBEDTLS_ERR_SSL_WANT_READ && result != MBEDTLS_ERR_SSL_WANT_WRITE) {
            set_tls_io_error("TLS write", &tls->ssl, result);
            return -1;
        }
    }
    return written > INT32_MAX ? INT32_MAX : (int)written;
}

int wl_tls_recv(wl_tls_t *tls, void *buf, size_t len) {
    if (tls == NULL) {
        return -1;
    }
    for (;;) {
        int result = mbedtls_ssl_read(&tls->ssl, buf, len);
        if (result >= 0) {
            return result;
        }
        if (result == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY ||
            tls_is_close_notify(&tls->ssl)) {
            return 0;
        }
        if (result != MBEDTLS_ERR_SSL_WANT_READ && result != MBEDTLS_ERR_SSL_WANT_WRITE) {
            set_tls_io_error("TLS read", &tls->ssl, result);
            return -1;
        }
    }
}

void wl_tls_close(wl_tls_t *tls) {
    if (tls == NULL) {
        return;
    }
    (void)mbedtls_ssl_close_notify(&tls->ssl);
    tls_destroy(tls);
}
