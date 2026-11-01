#include "unigate_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "cJSON.h"
#include "mbedtls/sha256.h"

static char *duplicate_text(const char *text) {
    size_t len;
    char *copy;

    if (text == NULL) {
        text = "";
    }
    len = strlen(text);
    copy = malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, text, len + 1);
    }
    return copy;
}

static int is_sensitive_key(const char *key) {
    static const char *const fields[] = {
        "data",
        "track1",
        "track2",
        "track3",
        "magneprint",
        "ksn",
        "pan",
        "cvv",
        "keydata",
        "applepaytoken",
        "googlepaytoken",
    };
    size_t i;

    for (i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        if (strcasecmp(key, fields[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static char *redacted_value(const char *value) {
    unsigned char digest[32];
    char buffer[128];
    int written;

    if (mbedtls_sha256((const unsigned char *)value, strlen(value), digest, 0) != 0) {
        return NULL;
    }
    written = snprintf(
        buffer,
        sizeof(buffer),
        "<redacted len=%zu sha256=%02x%02x%02x%02x%02x%02x>",
        strlen(value),
        digest[0], digest[1], digest[2],
        digest[3], digest[4], digest[5]
    );
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return NULL;
    }
    return duplicate_text(buffer);
}

static int redact_tree(cJSON *item) {
    cJSON *child;

    if (item == NULL) {
        return -1;
    }
    if (cJSON_IsObject(item)) {
        for (child = item->child; child != NULL; child = child->next) {
            if (child->string != NULL && cJSON_IsString(child) &&
                child->valuestring != NULL && child->valuestring[0] != '\0' &&
                is_sensitive_key(child->string)) {
                char *value = redacted_value(child->valuestring);
                if (value == NULL) {
                    return -1;
                }
                free(child->valuestring);
                child->valuestring = value;
                continue;
            }
            if (redact_tree(child) != 0) {
                return -1;
            }
        }
    } else if (cJSON_IsArray(item)) {
        for (child = item->child; child != NULL; child = child->next) {
            if (redact_tree(child) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

char *unigate_redact_json(const char *body, size_t body_len) {
    cJSON *root;
    char *output;

    if (body == NULL || body_len == 0) {
        return duplicate_text("<not-json>");
    }
    root = cJSON_ParseWithLength(body, body_len);
    if (root == NULL) {
        return duplicate_text("<not-json>");
    }
    if (redact_tree(root) != 0) {
        cJSON_Delete(root);
        return duplicate_text("<redact-failed>");
    }
    output = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (output == NULL) {
        return duplicate_text("<redact-failed>");
    }
    return output;
}

static int assign_string(cJSON *root, const char *field, char **target) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, field);

    if (item == NULL) {
        return 0;
    }
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return 0;
    }
    free(*target);
    *target = duplicate_text(item->valuestring);
    return *target != NULL ? 0 : -1;
}

int unigate_parse_fault(const char *body, size_t body_len, unigate_response *response) {
    cJSON *root;

    if (body == NULL || response == NULL || body_len == 0) {
        return -1;
    }
    root = cJSON_ParseWithLength(body, body_len);
    if (root == NULL || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return -1;
    }

    if (assign_string(root, "code", &response->fault_code) != 0 ||
        assign_string(root, "message", &response->fault_message) != 0 ||
        assign_string(root, "traceID", &response->fault_trace_id) != 0) {
        cJSON_Delete(root);
        return -1;
    }
    cJSON_Delete(root);
    return 0;
}
