GOOS ?= $(shell go env GOOS)
GOARCH ?= $(shell go env GOARCH)
TARGET := $(GOOS)_$(GOARCH)
OBJ_DIR ?= .build/$(TARGET)
LIB_DIR ?= lib
TARGET_LIB_DIR := $(LIB_DIR)/$(TARGET)

ifeq ($(TARGET),linux_amd64)
CC := x86_64-unknown-linux-musl-gcc
AR := x86_64-unknown-linux-musl-ar
RANLIB := x86_64-unknown-linux-musl-ranlib
else
CC ?= cc
AR ?= ar
RANLIB ?= ranlib
endif

SUPPORTED_TARGETS := darwin_amd64 darwin_arm64 linux_amd64 linux_arm64
ifeq ($(filter $(TARGET),$(SUPPORTED_TARGETS)),)
$(error unsupported target $(TARGET); supported targets: $(SUPPORTED_TARGETS))
endif

ifeq ($(TARGET),linux_arm64)
ifeq ($(origin CC),default)
$(error linux_arm64 requires an aarch64 musl cross compiler, for example CC=aarch64-linux-musl-gcc)
endif
endif

UNIGATE_CPPFLAGS := -I. -Ic -Iweblite -Icjson \
	-Imbedtls_config -Imbedtls/include -Imbedtls/library
UNIGATE_CFLAGS := -std=c11 -O2 -fPIC -Wall -Wextra \
	-D_GNU_SOURCE -DWEBLITE_POSIX=1 -DWEBLITE_HAS_TLS=1 -pthread
UNIGATE_CFLAGS += $(EXTRA_CFLAGS)

UNIGATE_SRCS := \
	c/unigate_client.c \
	c/unigate_http.c \
	c/unigate_json.c \
	c/unigate_transaction.c \
	c/unigate_tls_mbedtls.c

VENDOR_SRCS := \
	weblite/weblite.c \
	cjson/cJSON.c

MBEDTLS_SRCS := \
	mbedtls/library/aes.c \
	mbedtls/library/asn1parse.c \
	mbedtls/library/asn1write.c \
	mbedtls/library/base64.c \
	mbedtls/library/bignum.c \
	mbedtls/library/bignum_core.c \
	mbedtls/library/ccm.c \
	mbedtls/library/chacha20.c \
	mbedtls/library/chachapoly.c \
	mbedtls/library/cipher.c \
	mbedtls/library/cipher_wrap.c \
	mbedtls/library/constant_time.c \
	mbedtls/library/ctr_drbg.c \
	mbedtls/library/ecdh.c \
	mbedtls/library/ecdsa.c \
	mbedtls/library/ecp.c \
	mbedtls/library/ecp_curves.c \
	mbedtls/library/entropy.c \
	mbedtls/library/entropy_poll.c \
	mbedtls/library/gcm.c \
	mbedtls/library/md.c \
	mbedtls/library/oid.c \
	mbedtls/library/pem.c \
	mbedtls/library/pk.c \
	mbedtls/library/pk_ecc.c \
	mbedtls/library/pk_wrap.c \
	mbedtls/library/pkparse.c \
	mbedtls/library/platform.c \
	mbedtls/library/platform_util.c \
	mbedtls/library/poly1305.c \
	mbedtls/library/rsa.c \
	mbedtls/library/rsa_alt_helpers.c \
	mbedtls/library/sha1.c \
	mbedtls/library/sha256.c \
	mbedtls/library/sha512.c \
	mbedtls/library/ssl_ciphersuites.c \
	mbedtls/library/ssl_client.c \
	mbedtls/library/ssl_msg.c \
	mbedtls/library/ssl_tls.c \
	mbedtls/library/ssl_tls12_client.c \
	mbedtls/library/x509.c \
	mbedtls/library/x509_crt.c

OBJS := $(UNIGATE_SRCS:c/%.c=$(OBJ_DIR)/%.o)
OBJS += $(VENDOR_SRCS:%.c=$(OBJ_DIR)/%.o)
OBJS += $(MBEDTLS_SRCS:mbedtls/library/%.c=$(OBJ_DIR)/mbedtls_%.o)
LIB := $(TARGET_LIB_DIR)/libunigate.a
DEPS := $(OBJS:.o=.d)

.PHONY: all static test clean clean-target clean-all

all: $(LIB)

static: $(LIB)

test: $(LIB)
	@mkdir -p $(OBJ_DIR)
	$(CC) -I. tests/smoke.c $(LIB) -pthread -o $(OBJ_DIR)/smoke
	$(OBJ_DIR)/smoke

$(LIB): $(OBJS) | $(TARGET_LIB_DIR)
	$(AR) rcs $@ $(OBJS)
	$(RANLIB) $@

$(OBJ_DIR)/%.o: c/%.c
	@mkdir -p $(dir $@)
	$(CC) $(UNIGATE_CPPFLAGS) $(UNIGATE_CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/weblite/%.o: weblite/%.c
	@mkdir -p $(dir $@)
	$(CC) $(UNIGATE_CPPFLAGS) $(UNIGATE_CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/cjson/%.o: cjson/%.c
	@mkdir -p $(dir $@)
	$(CC) $(UNIGATE_CPPFLAGS) $(UNIGATE_CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/mbedtls_%.o: mbedtls/library/%.c
	@mkdir -p $(dir $@)
	$(CC) $(UNIGATE_CPPFLAGS) $(UNIGATE_CFLAGS) -MMD -MP -c $< -o $@

$(TARGET_LIB_DIR):
	@mkdir -p $@

clean: clean-target

clean-target:
	rm -rf $(OBJ_DIR) $(TARGET_LIB_DIR)

clean-all:
	rm -rf .build $(LIB_DIR)

-include $(DEPS)
