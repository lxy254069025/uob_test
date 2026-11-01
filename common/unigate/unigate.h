#ifndef UOPENBOX_UNIGATE_H
#define UOPENBOX_UNIGATE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum unigate_operation {
    UNIGATE_OP_EMV = 1,
    UNIGATE_OP_TOKEN,
    UNIGATE_OP_CARD_SWIPE,
    UNIGATE_OP_KEYPAD,
    UNIGATE_OP_MANUAL,
    UNIGATE_OP_INAPP_APPLE_PAY,
    UNIGATE_OP_TEC_APPLE_PAY,
    UNIGATE_OP_GOOGLE_PAY,
    UNIGATE_OP_INCREMENTAL_AUTHORIZE,
    UNIGATE_OP_TIP_ADJUST,
    UNIGATE_OP_CAPTURE,
    UNIGATE_OP_REFUND,
    UNIGATE_OP_VOID,
    UNIGATE_OP_GET_BY_MAG_TRAN_ID,
    UNIGATE_OP_GET_BY_CUSTOMER_TRAN_ID,
    UNIGATE_OP_APPLE_TAP_TO_PAY_READER
} unigate_operation;

typedef void (*unigate_log_callback)(const char *message, void *user_data);

typedef struct unigate_client_options {
    const char *base_url;
    const char *customer_code;
    const char *user_name;
    const char *password;
    const char *ca_path;
    long timeout_ms;
    int log_request_body;
    unigate_log_callback log_callback;
    void *log_user_data;
} unigate_client_options;

typedef struct unigate_key_value {
    const char *key;
    const char *value;
} unigate_key_value;

typedef struct unigate_request {
    unigate_operation operation;
    const char *customer_transaction_id;
    const char *transaction_type;
    double amount;
    const char *data_type;
    const char *data;
    const char *payment_type;
    const char *token;

    const char *track1;
    const char *track2;
    const char *track3;
    const char *magne_print;
    const char *magne_print_status;
    const char *ksn;
    const char *key_variant;
    const char *cvv;
    const char *zip;
    const char *is_fallback;

    const char *address_line1;
    const char *address_line2;
    const char *city;
    const char *state;
    const char *country;
    const char *name_on_card;
    const char *pan;
    const char *expiration_date;

    const char *apple_pay_token;
    const char *google_pay_token;
    const char *reference_mag_tran_id;
    double tip_amount;
    int has_amount;
    const unigate_key_value *transaction_input_details;
    size_t transaction_input_detail_count;

    const char *mag_tran_id;
    const char *customer_transaction_id_query;
    const char *payment_card_reader_identifier;
} unigate_request;

typedef struct unigate_response {
    int status_code;
    char *method;
    char *path;
    char *body;
    size_t body_len;
    char *fault_code;
    char *fault_message;
    char *fault_trace_id;
    char *error;
} unigate_response;

/*
 * Execute one Unigate operation.
 *
 * The C SDK builds the operation-specific URL and JSON request, performs
 * HTTP/TLS, parses Magensa fault fields, and owns all response memory.
 *
 * Return 0 when an HTTP response was received, including non-2xx responses.
 * Return -1 on request building, network, TLS, protocol, or allocation failure.
 */
int unigate_execute_request(const unigate_client_options *options,
                            const unigate_request *request,
                            unigate_response *response);

void unigate_response_free(unigate_response *response);

/*
 * Redact sensitive JSON string fields and return a newly allocated JSON
 * document. The caller must release it with unigate_free().
 */
char *unigate_redact_json(const char *body, size_t body_len);

void unigate_free(void *ptr);

const char *unigate_default_base_url(void);

#ifdef __cplusplus
}
#endif

#endif
