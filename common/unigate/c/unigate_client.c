#include "unigate_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *unigate_default_base_url(void) {
    return "https://svc1.magensa.net/Unigate";
}

int unigate_execute_request(const unigate_client_options *options,
                            const unigate_request *request,
                            unigate_response *response) {
    char *method = NULL;
    char *url = NULL;
    char *path = NULL;
    char *body = NULL;
    char *error = NULL;
    int result;

    if (response == NULL) {
        return -1;
    }
    memset(response, 0, sizeof(*response));
    if (options == NULL || request == NULL) {
        response->error = strdup("invalid request arguments");
        return -1;
    }
    if (options->customer_code == NULL || options->user_name == NULL ||
        options->password == NULL) {
        response->error = strdup("missing client credentials");
        return -1;
    }
    if (options->timeout_ms <= 0) {
        response->error = strdup("invalid request timeout");
        return -1;
    }

    if (unigate_build_request(
            options,
            request,
            &method,
            &url,
            &path,
            &body,
            &error
        ) != 0) {
        response->error = error != NULL ? error : strdup("building Unigate request");
        unigate_free(method);
        unigate_free(url);
        unigate_free(path);
        unigate_free(body);
        return -1;
    }
    unigate_free(error);

    if (options->log_request_body && options->log_callback != NULL && body != NULL) {
        char *redacted = unigate_redact_json(body, strlen(body));
        if (redacted != NULL) {
            size_t message_len = strlen(method) + strlen(path) + strlen(redacted) + 32;
            char *message = malloc(message_len);
            if (message != NULL) {
                snprintf(
                    message,
                    message_len,
                    "unigate: %s %s body=%s",
                    method,
                    path,
                    redacted
                );
                options->log_callback(message, options->log_user_data);
                free(message);
            }
            unigate_free(redacted);
        }
    }

    result = unigate_http_request(
        method,
        url,
        options->customer_code,
        options->user_name,
        options->password,
        body,
        options->timeout_ms,
        options->ca_path,
        response
    );
    response->method = strdup(method);
    response->path = strdup(path);

    unigate_free(method);
    unigate_free(url);
    unigate_free(path);
    unigate_free(body);
    return result;
}
