MUSL_CC ?= x86_64-unknown-linux-musl-gcc
OUTPUT ?= bin/martbox-api-linux-amd64

.PHONY: build-linux-amd64

build-linux-amd64:
	@command -v $(MUSL_CC) >/dev/null || { \
		echo "missing $(MUSL_CC); install the x86_64 musl cross toolchain" >&2; \
		exit 1; \
	}
	$(MAKE) -C ../common/unigate \
		GOOS=linux GOARCH=amd64 static EXTRA_CFLAGS=-Werror
	@mkdir -p $(dir $(OUTPUT))
	CGO_ENABLED=1 GOOS=linux GOARCH=amd64 CC=$(MUSL_CC) \
		go build -a \
		-ldflags '-linkmode external -extldflags "-static"' \
		-o $(OUTPUT) .
	@go version -m $(OUTPUT) | grep -q 'CGO_ENABLED=1' || { \
		echo "refusing cgo-disabled binary: $(OUTPUT)" >&2; \
		exit 1; \
	}
	@file $(OUTPUT)
