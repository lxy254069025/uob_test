#include "unigate_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

static void set_error(char **target, const char *fmt, ...) {
    va_list args;
    va_list copy;
    int needed;
    char *message;

    if (target == NULL || *target != NULL) {
        return;
    }
    va_start(args, fmt);
    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed >= 0) {
        message = malloc((size_t)needed + 1);
        if (message != NULL) {
            vsnprintf(message, (size_t)needed + 1, fmt, args);
            *target = message;
        }
    }
    va_end(args);
}

static int add_string(cJSON *object, const char *name, const char *value) {
    return cJSON_AddStringToObject(object, name, value != NULL ? value : "") != NULL ? 0 : -1;
}

static int add_number(cJSON *object, const char *name, double value) {
    return cJSON_AddNumberToObject(object, name, value) != NULL ? 0 : -1;
}

static int add_transaction_input(
    cJSON *object,
    const char *transaction_type,
    double amount
) {
    cJSON *input = cJSON_AddObjectToObject(object, "transactionInput");
    return input != NULL &&
           add_string(input, "transactionType", transaction_type) == 0 &&
           add_number(input, "amount", amount) == 0
               ? 0
               : -1;
}

static cJSON *build_emv(const unigate_request *request) {
    cJSON *root = cJSON_CreateObject();
    cJSON *data_input;
    cJSON *encrypted_data;

    if (root == NULL ||
        add_string(root, "customerTransactionID", request->customer_transaction_id) != 0 ||
        add_transaction_input(root, request->transaction_type, request->amount) != 0) {
        cJSON_Delete(root);
        return NULL;
    }
    data_input = cJSON_AddObjectToObject(root, "dataInput");
    encrypted_data = data_input != NULL
                         ? cJSON_AddObjectToObject(data_input, "encryptedData")
                         : NULL;
    if (data_input == NULL || encrypted_data == NULL ||
        add_string(encrypted_data, "dataType", request->data_type) != 0 ||
        add_string(encrypted_data, "data", request->data) != 0 ||
        add_string(data_input, "paymentType", request->payment_type) != 0) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}

static cJSON *build_token(const unigate_request *request) {
    cJSON *root = cJSON_CreateObject();

    if (root == NULL ||
        add_string(root, "customerTransactionID", request->customer_transaction_id) != 0 ||
        add_transaction_input(root, request->transaction_type, request->amount) != 0 ||
        add_string(root, "token", request->token) != 0) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}

static cJSON *build_card_swipe(const unigate_request *request) {
    cJSON *root = cJSON_CreateObject();
    cJSON *input;
    cJSON *swipe;

    if (root == NULL ||
        add_string(root, "customerTransactionID", request->customer_transaction_id) != 0 ||
        add_transaction_input(root, request->transaction_type, request->amount) != 0) {
        cJSON_Delete(root);
        return NULL;
    }
    input = cJSON_AddObjectToObject(root, "cardSwipeInput");
    swipe = input != NULL ? cJSON_AddObjectToObject(input, "encryptedCardSwipe") : NULL;
    if (input == NULL || swipe == NULL ||
        add_string(swipe, "track1", request->track1) != 0 ||
        add_string(swipe, "track2", request->track2) != 0 ||
        add_string(swipe, "track3", request->track3) != 0 ||
        add_string(swipe, "magnePrint", request->magne_print) != 0 ||
        add_string(swipe, "magnePrintStatus", request->magne_print_status) != 0 ||
        add_string(swipe, "ksn", request->ksn) != 0 ||
        add_string(swipe, "keyVariant", request->key_variant) != 0 ||
        add_string(input, "cvv", request->cvv) != 0 ||
        add_string(input, "zip", request->zip) != 0 ||
        add_string(input, "paymentType", request->payment_type) != 0 ||
        add_string(input, "isFallBack", request->is_fallback) != 0) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}

static cJSON *build_keypad(const unigate_request *request) {
    cJSON *root = cJSON_CreateObject();
    cJSON *input;
    cJSON *entry;

    if (root == NULL ||
        add_string(root, "customerTransactionID", request->customer_transaction_id) != 0 ||
        add_transaction_input(root, request->transaction_type, request->amount) != 0) {
        cJSON_Delete(root);
        return NULL;
    }
    input = cJSON_AddObjectToObject(root, "keyPadEntryInput");
    entry = input != NULL ? cJSON_AddObjectToObject(input, "encryptedKeyPadEntry") : NULL;
    if (input == NULL || entry == NULL ||
        add_string(entry, "track1", request->track1) != 0 ||
        add_string(entry, "track2", request->track2) != 0 ||
        add_string(entry, "track3", request->track3) != 0 ||
        add_string(entry, "ksn", request->ksn) != 0 ||
        add_string(entry, "keyVariant", request->key_variant) != 0 ||
        add_string(input, "cvv", request->cvv) != 0 ||
        add_string(input, "zip", request->zip) != 0 ||
        add_string(input, "paymentType", request->payment_type) != 0) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}

static cJSON *build_manual(const unigate_request *request) {
    cJSON *root = cJSON_CreateObject();
    cJSON *input;

    if (root == NULL ||
        add_string(root, "customerTransactionID", request->customer_transaction_id) != 0 ||
        add_transaction_input(root, request->transaction_type, request->amount) != 0) {
        cJSON_Delete(root);
        return NULL;
    }
    input = cJSON_AddObjectToObject(root, "manualEntryInput");
    if (input == NULL ||
        add_string(input, "addressLine1", request->address_line1) != 0 ||
        add_string(input, "addressLine2", request->address_line2) != 0 ||
        add_string(input, "city", request->city) != 0 ||
        add_string(input, "state", request->state) != 0 ||
        add_string(input, "country", request->country) != 0 ||
        add_string(input, "zip", request->zip) != 0 ||
        add_string(input, "nameOnCard", request->name_on_card) != 0 ||
        add_string(input, "pan", request->pan) != 0 ||
        add_string(input, "expirationDate", request->expiration_date) != 0 ||
        add_string(input, "cvv", request->cvv) != 0) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}

static cJSON *build_wallet(const unigate_request *request, const char *field) {
    cJSON *root = cJSON_CreateObject();
    const char *token = request->operation == UNIGATE_OP_GOOGLE_PAY
                            ? request->google_pay_token
                            : request->apple_pay_token;

    if (root == NULL ||
        add_string(root, "customerTransactionID", request->customer_transaction_id) != 0 ||
        add_transaction_input(root, request->transaction_type, request->amount) != 0 ||
        add_string(root, field, token) != 0) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}

static cJSON *build_reference_amount(const unigate_request *request) {
    cJSON *root = cJSON_CreateObject();

    if (root == NULL ||
        add_string(root, "referenceMagTranID", request->reference_mag_tran_id) != 0 ||
        add_number(root, "amount", request->amount) != 0) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}

static cJSON *build_tip_adjust(const unigate_request *request) {
    cJSON *root = cJSON_CreateObject();
    cJSON *details;
    size_t i;

    if (root == NULL ||
        add_string(root, "referenceMagTranID", request->reference_mag_tran_id) != 0 ||
        add_number(root, "tipAmount", request->tip_amount) != 0) {
        cJSON_Delete(root);
        return NULL;
    }
    if (request->transaction_input_detail_count == 0) {
        return root;
    }
    details = cJSON_AddArrayToObject(root, "transactionInputDetails");
    if (details == NULL) {
        cJSON_Delete(root);
        return NULL;
    }
    for (i = 0; i < request->transaction_input_detail_count; i++) {
        cJSON *item = cJSON_CreateObject();
        if (item == NULL ||
            add_string(item, "key", request->transaction_input_details[i].key) != 0 ||
            add_string(item, "value", request->transaction_input_details[i].value) != 0 ||
            !cJSON_AddItemToArray(details, item)) {
            cJSON_Delete(item);
            cJSON_Delete(root);
            return NULL;
        }
    }
    return root;
}

static cJSON *build_void(const unigate_request *request) {
    cJSON *root = cJSON_CreateObject();

    if (root == NULL ||
        add_string(root, "referenceMagTranID", request->reference_mag_tran_id) != 0 ||
        (request->has_amount && add_number(root, "amount", request->amount) != 0)) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}

static cJSON *build_reader(const unigate_request *request) {
    cJSON *root = cJSON_CreateObject();

    if (root == NULL ||
        add_string(
            root,
            "paymentCardReaderIdentifier",
            request->payment_card_reader_identifier
        ) != 0) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}

static char *encode_component(const char *value) {
    static const char hex[] = "0123456789ABCDEF";
    const unsigned char *cur = (const unsigned char *)(value != NULL ? value : "");
    size_t len = strlen((const char *)cur);
    char *encoded = malloc(len * 3 + 1);
    size_t out = 0;

    if (encoded == NULL) {
        return NULL;
    }
    while (*cur != '\0') {
        unsigned char ch = *cur++;
        if ((ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            encoded[out++] = (char)ch;
        } else {
            encoded[out++] = '%';
            encoded[out++] = hex[ch >> 4];
            encoded[out++] = hex[ch & 0x0f];
        }
    }
    encoded[out] = '\0';
    return encoded;
}

static int set_route(
    const unigate_request *request,
    const char **method,
    char **path,
    char **error
) {
    char *encoded;
    size_t path_len;

    *method = "POST";
    switch (request->operation) {
    case UNIGATE_OP_EMV:
        *path = strdup("/api/Transaction/EMV");
        break;
    case UNIGATE_OP_TOKEN:
        *path = strdup("/api/Transaction/Token");
        break;
    case UNIGATE_OP_CARD_SWIPE:
        *path = strdup("/api/Transaction/CardSwipe");
        break;
    case UNIGATE_OP_KEYPAD:
        *path = strdup("/api/Transaction/KeyPadEntry");
        break;
    case UNIGATE_OP_MANUAL:
        *path = strdup("/api/Transaction/ManualEntry");
        break;
    case UNIGATE_OP_INAPP_APPLE_PAY:
        *path = strdup("/api/Transaction/InAppApplePay");
        break;
    case UNIGATE_OP_TEC_APPLE_PAY:
        *path = strdup("/api/Transaction/TECApplePay");
        break;
    case UNIGATE_OP_GOOGLE_PAY:
        *path = strdup("/api/Transaction/GooglePay");
        break;
    case UNIGATE_OP_INCREMENTAL_AUTHORIZE:
        *path = strdup("/api/ReferenceTransaction/AUTHORIZE");
        break;
    case UNIGATE_OP_TIP_ADJUST:
        *path = strdup("/api/ReferenceTransaction/TIPADJUST");
        break;
    case UNIGATE_OP_CAPTURE:
        *path = strdup("/api/ReferenceTransaction/CAPTURE");
        break;
    case UNIGATE_OP_REFUND:
        *path = strdup("/api/ReferenceTransaction/REFUND");
        break;
    case UNIGATE_OP_VOID:
        *path = strdup("/api/ReferenceTransaction/VOID");
        break;
    case UNIGATE_OP_APPLE_TAP_TO_PAY_READER:
        *path = strdup("/api/AppleTapToPayToken/PaymentCardReader");
        break;
    case UNIGATE_OP_GET_BY_MAG_TRAN_ID:
        *method = "GET";
        encoded = encode_component(request->mag_tran_id);
        if (encoded == NULL) {
            set_error(error, "encoding magTranID");
            return -1;
        }
        path_len = strlen("/api/Transaction/") + strlen(encoded) + 1;
        *path = malloc(path_len);
        if (*path != NULL) {
            snprintf(*path, path_len, "/api/Transaction/%s", encoded);
        }
        free(encoded);
        break;
    case UNIGATE_OP_GET_BY_CUSTOMER_TRAN_ID:
        *method = "GET";
        encoded = encode_component(request->customer_transaction_id_query);
        if (encoded == NULL) {
            set_error(error, "encoding custTranID");
            return -1;
        }
        path_len = strlen("/api/Transaction?custTranID=") + strlen(encoded) + 1;
        *path = malloc(path_len);
        if (*path != NULL) {
            snprintf(*path, path_len, "/api/Transaction?custTranID=%s", encoded);
        }
        free(encoded);
        break;
    default:
        set_error(error, "unsupported operation");
        return -1;
    }
    if (*path == NULL) {
        set_error(error, "allocating request path");
        return -1;
    }
    return 0;
}

static cJSON *build_body(const unigate_request *request, char **error) {
    switch (request->operation) {
    case UNIGATE_OP_EMV:
        return build_emv(request);
    case UNIGATE_OP_TOKEN:
        return build_token(request);
    case UNIGATE_OP_CARD_SWIPE:
        return build_card_swipe(request);
    case UNIGATE_OP_KEYPAD:
        return build_keypad(request);
    case UNIGATE_OP_MANUAL:
        return build_manual(request);
    case UNIGATE_OP_INAPP_APPLE_PAY:
        return build_wallet(request, "applePayToken");
    case UNIGATE_OP_TEC_APPLE_PAY:
        return build_wallet(request, "applePayToken");
    case UNIGATE_OP_GOOGLE_PAY:
        return build_wallet(request, "googlePayToken");
    case UNIGATE_OP_INCREMENTAL_AUTHORIZE:
    case UNIGATE_OP_CAPTURE:
    case UNIGATE_OP_REFUND:
        return build_reference_amount(request);
    case UNIGATE_OP_TIP_ADJUST:
        return build_tip_adjust(request);
    case UNIGATE_OP_VOID:
        return build_void(request);
    case UNIGATE_OP_APPLE_TAP_TO_PAY_READER:
        return build_reader(request);
    case UNIGATE_OP_GET_BY_MAG_TRAN_ID:
    case UNIGATE_OP_GET_BY_CUSTOMER_TRAN_ID:
        return NULL;
    default:
        set_error(error, "unsupported operation");
        return NULL;
    }
}

int unigate_build_request(
    const unigate_client_options *options,
    const unigate_request *request,
    char **method,
    char **url,
    char **path,
    char **body,
    char **error
) {
    const char *base_url;
    const char *request_method;
    cJSON *json = NULL;
    size_t url_len;

    if (method == NULL || url == NULL || path == NULL || body == NULL) {
        set_error(error, "invalid output arguments");
        return -1;
    }
    *method = NULL;
    *url = NULL;
    *path = NULL;
    *body = NULL;
    if (options == NULL || request == NULL) {
        set_error(error, "invalid request arguments");
        return -1;
    }
    if (set_route(request, &request_method, path, error) != 0) {
        return -1;
    }

    base_url = options->base_url != NULL && options->base_url[0] != '\0'
                   ? options->base_url
                   : unigate_default_base_url();
    url_len = strlen(base_url) + strlen(*path) + 1;
    *url = malloc(url_len);
    *method = strdup(request_method);
    if (*url == NULL || *method == NULL) {
        set_error(error, "allocating request URL");
        goto failure;
    }
    snprintf(*url, url_len, "%s%s", base_url, *path);

    json = build_body(request, error);
    if (json == NULL) {
        if (*error == NULL) {
            goto success;
        }
        goto failure;
    }
    *body = cJSON_PrintUnformatted(json);
    if (*body == NULL) {
        set_error(error, "encoding request body");
        goto failure;
    }

success:
    cJSON_Delete(json);
    return 0;

failure:
    cJSON_Delete(json);
    unigate_free(*method);
    unigate_free(*url);
    unigate_free(*path);
    unigate_free(*body);
    *method = NULL;
    *url = NULL;
    *path = NULL;
    *body = NULL;
    return -1;
}
