#include "unigate_internal.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "weblite/weblite.h"

extern void unigate_tls_set_request_ca_path(const char *path);

static pthread_mutex_t weblite_init_lock = PTHREAD_MUTEX_INITIALIZER;
static int weblite_ready;

static char *duplicate_string(const char *text) {
    size_t len;
    char *copy;

    if (text == NULL) {
        return NULL;
    }
    len = strlen(text);
    copy = malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, text, len + 1);
    }
    return copy;
}

void unigate_response_free(unigate_response *response) {
    if (response == NULL) {
        return;
    }
    free(response->body);
    free(response->method);
    free(response->path);
    free(response->fault_code);
    free(response->fault_message);
    free(response->fault_trace_id);
    free(response->error);
    memset(response, 0, sizeof(*response));
}

void unigate_free(void *ptr) {
    free(ptr);
}

static int file_exists(const char *path) {
    FILE *file;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    fclose(file);
    return 1;
}

static const char *default_ca_path(const char *requested) {
    static const char *const paths[] = {
        "/etc/ssl/cert.pem",
        "/etc/ssl/certs/ca-certificates.crt",
        "/etc/pki/tls/certs/ca-bundle.crt",
        "/opt/homebrew/etc/ca-certificates/cert.pem",
    };
    const char *environment;
    size_t i;

    if (file_exists(requested)) {
        return requested;
    }
    environment = getenv("UNIGATE_CA_BUNDLE");
    if (file_exists(environment)) {
        return environment;
    }
    for (i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        if (file_exists(paths[i])) {
            return paths[i];
        }
    }
    return NULL;
}

static int ensure_weblite(void) {
    int result = 0;

    pthread_mutex_lock(&weblite_init_lock);
    if (!weblite_ready) {
        weblite_config_t config = weblite_config_defaults();
        config.default_transport = WEBLITE_TRANSPORT_HTTP_ONLY;
        config.default_timeout_ms = 30000;
        config.default_max_redirects = 0;
        config.tls_verify = 1;
        config.ca_path = NULL;
        config.thread_pool_size = 0;
        result = weblite_init(&config) == WEBLITE_OK ? 0 : -1;
        weblite_ready = result == 0;
    }
    pthread_mutex_unlock(&weblite_init_lock);
    return result;
}

static char *base64_encode(const unsigned char *data, size_t len) {
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t out_len = 4 * ((len + 2) / 3);
    char *out;
    size_t i;
    size_t j = 0;

    out = malloc(out_len + 1);
    if (out == NULL) {
        return NULL;
    }
    for (i = 0; i + 2 < len; i += 3) {
        uint32_t value = ((uint32_t)data[i] << 16) |
                         ((uint32_t)data[i + 1] << 8) |
                         (uint32_t)data[i + 2];
        out[j++] = alphabet[(value >> 18) & 0x3f];
        out[j++] = alphabet[(value >> 12) & 0x3f];
        out[j++] = alphabet[(value >> 6) & 0x3f];
        out[j++] = alphabet[value & 0x3f];
    }
    if (i < len) {
        uint32_t value = (uint32_t)data[i] << 16;
        out[j++] = alphabet[(value >> 18) & 0x3f];
        if (i + 1 < len) {
            value |= (uint32_t)data[i + 1] << 8;
            out[j++] = alphabet[(value >> 12) & 0x3f];
            out[j++] = alphabet[(value >> 6) & 0x3f];
            out[j++] = '=';
        } else {
            out[j++] = alphabet[(value >> 12) & 0x3f];
            out[j++] = '=';
            out[j++] = '=';
        }
    }
    out[j] = '\0';
    return out;
}

static char *build_authorization(const char *customer_code, const char *user_name, const char *password) {
    size_t principal_len = strlen(customer_code) + 1 + strlen(user_name);
    size_t plain_len = principal_len + 1 + strlen(password);
    unsigned char *plain;
    char *encoded;
    char *header;
    size_t header_len;

    plain = malloc(plain_len + 1);
    if (plain == NULL) {
        return NULL;
    }
    snprintf((char *)plain, plain_len + 1, "%s/%s:%s", customer_code, user_name, password);
    encoded = base64_encode(plain, plain_len);
    memset(plain, 0, plain_len + 1);
    free(plain);
    if (encoded == NULL) {
        return NULL;
    }
    header_len = strlen(encoded) + sizeof("Basic ");
    header = malloc(header_len);
    if (header != NULL) {
        snprintf(header, header_len, "Basic %s", encoded);
    }
    free(encoded);
    return header;
}

static int method_from_string(const char *method, weblite_method_t *result) {
    if (strcasecmp(method, "GET") == 0) {
        *result = WEBLITE_METHOD_GET;
    } else if (strcasecmp(method, "POST") == 0) {
        *result = WEBLITE_METHOD_POST;
    } else if (strcasecmp(method, "PUT") == 0) {
        *result = WEBLITE_METHOD_PUT;
    } else if (strcasecmp(method, "DELETE") == 0) {
        *result = WEBLITE_METHOD_DELETE;
    } else if (strcasecmp(method, "PATCH") == 0) {
        *result = WEBLITE_METHOD_PATCH;
    } else if (strcasecmp(method, "HEAD") == 0) {
        *result = WEBLITE_METHOD_HEAD;
    } else if (strcasecmp(method, "OPTIONS") == 0) {
        *result = WEBLITE_METHOD_OPTIONS;
    } else {
        return -1;
    }
    return 0;
}

static void set_weblite_error(unigate_response *response, weblite_error_t error, const char *detail) {
    const char *description = weblite_error_string(error);
    size_t len;

    if (detail != NULL && detail[0] != '\0') {
        len = strlen(description) + strlen(detail) + 3;
        response->error = malloc(len);
        if (response->error != NULL) {
            snprintf(response->error, len, "%s: %s", description, detail);
        }
    } else {
        response->error = duplicate_string(description);
    }
}

int unigate_http_request(
    const char *method,
    const char *url,
    const char *auth_customer_code,
    const char *auth_user_name,
    const char *password,
    const char *body,
    long timeout_ms,
    const char *ca_path,
    unigate_response *response
) {
    weblite_method_t weblite_method;
    weblite_request_t request;
    weblite_response_t *weblite_response = NULL;
    weblite_header_t headers[3];
    size_t header_count = 0;
    char *authorization = NULL;
    const char *selected_ca;

    if (response == NULL) {
        return -1;
    }
    memset(response, 0, sizeof(*response));
    if (method == NULL || url == NULL || auth_customer_code == NULL ||
        auth_user_name == NULL || password == NULL || timeout_ms <= 0) {
        response->error = duplicate_string("invalid request arguments");
        return -1;
    }
    if (method_from_string(method, &weblite_method) != 0) {
        response->error = duplicate_string("unsupported HTTP method");
        return -1;
    }

    selected_ca = default_ca_path(ca_path);
    if (ensure_weblite() != 0) {
        response->error = duplicate_string("initializing weblite");
        return -1;
    }
    authorization = build_authorization(auth_customer_code, auth_user_name, password);
    if (authorization == NULL) {
        response->error = duplicate_string("building Authorization header");
        return -1;
    }

    headers[header_count].name = "Accept";
    headers[header_count++].value = "application/json";
    headers[header_count].name = "Authorization";
    headers[header_count++].value = authorization;
    if (body != NULL) {
        headers[header_count].name = "Content-Type";
        headers[header_count++].value = "application/json";
    }

    memset(&request, 0, sizeof(request));
    request.url = url;
    request.method = weblite_method;
    request.headers = headers;
    request.header_count = header_count;
    request.body = body;
    request.body_len = body != NULL ? strlen(body) : 0;
    request.transport_mode = WEBLITE_TRANSPORT_HTTP_ONLY;
    request.timeout_ms = timeout_ms > UINT32_MAX ? UINT32_MAX : (uint32_t)timeout_ms;
    request.max_redirects = 0;

    unigate_tls_set_request_ca_path(selected_ca);
    if (weblite_request(&request, &weblite_response) != WEBLITE_OK) {
        unigate_tls_set_request_ca_path(NULL);
        set_weblite_error(
            response,
            weblite_response != NULL ? weblite_response->error : WEBLITE_ERR_TRANSPORT,
            weblite_response != NULL ? weblite_response->error_msg : NULL
        );
        memset(authorization, 0, strlen(authorization));
        free(authorization);
        weblite_response_free(weblite_response);
        return -1;
    }
    unigate_tls_set_request_ca_path(NULL);

    response->status_code = weblite_response->status_code;
    if (weblite_response->body != NULL && weblite_response->body_len > 0) {
        response->body = malloc(weblite_response->body_len + 1);
        if (response->body == NULL) {
            response->error = duplicate_string("allocating response body");
            goto failure;
        }
        memcpy(response->body, weblite_response->body, weblite_response->body_len);
        response->body[weblite_response->body_len] = '\0';
        response->body_len = weblite_response->body_len;
    }
    if (response->status_code < 200 || response->status_code >= 300) {
        (void)unigate_parse_fault(response->body, response->body_len, response);
    }

    memset(authorization, 0, strlen(authorization));
    free(authorization);
    weblite_response_free(weblite_response);
    return 0;

failure:
    unigate_tls_set_request_ca_path(NULL);
    memset(authorization, 0, strlen(authorization));
    free(authorization);
    weblite_response_free(weblite_response);
    return -1;
}
