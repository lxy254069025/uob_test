#include <stdio.h>
#include <string.h>

#include "unigate.h"

int main(void) {
    static const char body[] =
        "{\"encryptedData\":{\"dataType\":\"ARQC\",\"data\":\"SECRET\"}}";
    char *redacted;

    if (strcmp(unigate_default_base_url(), "https://svc1.magensa.net/Unigate") != 0) {
        fputs("unexpected default URL\n", stderr);
        return 1;
    }
    redacted = unigate_redact_json(body, sizeof(body) - 1);
    if (redacted == NULL) {
        fputs("redaction failed\n", stderr);
        return 1;
    }
    if (strstr(redacted, "SECRET") != NULL ||
        strstr(redacted, "<redacted len=6 sha256=") == NULL) {
        fprintf(stderr, "redaction result is invalid: %s\n", redacted);
        unigate_free(redacted);
        return 1;
    }
    unigate_free(redacted);
    puts("unigate C SDK smoke test passed");
    return 0;
}
