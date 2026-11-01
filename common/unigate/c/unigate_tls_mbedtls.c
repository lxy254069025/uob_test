#include <stddef.h>
#include <stdint.h>
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

void unigate_tls_set_request_ca_path(const char *path) {
    request_ca_path = path;
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

    if (tls == NULL || hostname == NULL) {
        free(tls);
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
        tls_destroy(tls);
        return NULL;
    }

    mbedtls_ssl_conf_rng(&tls->config, mbedtls_ctr_drbg_random, &tls->random);
    mbedtls_ssl_conf_min_tls_version(&tls->config, MBEDTLS_SSL_VERSION_TLS1_2);

    if (verify) {
        if (effective_ca_path == NULL || effective_ca_path[0] == '\0') {
            tls_destroy(tls);
            return NULL;
        }
        if (mbedtls_x509_crt_parse_file(&tls->ca_chain, effective_ca_path) != 0) {
            tls_destroy(tls);
            return NULL;
        }
        mbedtls_ssl_conf_authmode(&tls->config, MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_ca_chain(&tls->config, &tls->ca_chain, NULL);
    } else {
        mbedtls_ssl_conf_authmode(&tls->config, MBEDTLS_SSL_VERIFY_NONE);
    }

    if (mbedtls_ssl_setup(&tls->ssl, &tls->config) != 0 ||
        mbedtls_ssl_set_hostname(&tls->ssl, hostname) != 0) {
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

    if (result != 0 || (verify && mbedtls_ssl_get_verify_result(&tls->ssl) != 0)) {
        tls_destroy(tls);
        return NULL;
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
        if (result != MBEDTLS_ERR_SSL_WANT_READ && result != MBEDTLS_ERR_SSL_WANT_WRITE) {
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
