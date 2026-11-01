#ifndef UOPENBOX_UNIGATE_INTERNAL_H
#define UOPENBOX_UNIGATE_INTERNAL_H

#include "unigate.h"

int unigate_build_request(const unigate_client_options *options,
                          const unigate_request *request,
                          char **method,
                          char **url,
                          char **path,
                          char **body,
                          char **error);

int unigate_http_request(const char *method,
                         const char *url,
                         const char *auth_customer_code,
                         const char *auth_user_name,
                         const char *password,
                         const char *body,
                         long timeout_ms,
                         const char *ca_path,
                         unigate_response *response);

int unigate_parse_fault(const char *body, size_t body_len, unigate_response *response);

#endif
