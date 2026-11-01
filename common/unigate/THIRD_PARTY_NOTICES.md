# Third-party notices

The `common/unigate` C SDK compiles the following third-party source code into
`libunigate.a`. No system HTTP or SSL shared library is linked.

## weblite 1.0.0

- Files: `weblite/weblite.c`, `weblite/weblite.h`
- Purpose: HTTP/1.1 transport and response parsing
- License: see the upstream files and project distribution terms

## mbedTLS 3.6.4

- Files: `mbedtls/include/`, `mbedtls/library/`
- Purpose: TLS backend used by weblite
- License: Apache-2.0 OR GPL-2.0-or-later
- License text: `mbedtls/LICENSE`
- Build configuration: `unigate_mbedtls_config.h`

## cJSON 1.7.18

- Files: `cjson/cJSON.c`, `cjson/cJSON.h`
- Purpose: request redaction and Magensa fault response parsing
- License: MIT
- License text: `cjson/LICENSE`
