# Unigate C SDK 与 Go 调用说明

`common/unigate` 是 Magensa Unigate 的可移植 C SDK。接口路径、查询参数、请求 JSON、
HTTP/TLS、Magensa fault 解析和敏感字段脱敏均由 C 实现。

目录结构：

```text
common/unigate/
├── unigate.h                 公开 C 头文件
├── Makefile                  按 GOOS/GOARCH 构建静态库
├── c/                        Unigate C 实现
├── weblite/                  HTTP 源码
├── mbedtls/                  TLS 源码
├── cjson/                    JSON 源码
├── types.go                  C/Go 共用的公开类型
├── binding.go                Go cgo 绑定
├── binding_nocgo.go          CGO_ENABLED=0 时的明确错误实现
└── lib/<goos>_<goarch>/
    └── libunigate.a          当前目标的构建产物
```

## 构建静态库

当前 Makefile 使用 POSIX 平台实现，需要 C11 编译器、`make`、`ar` 和 `ranlib`。
默认从 `go env GOOS` 和 `go env GOARCH` 获取目标，并将对象文件和静态库按目标隔离。
支持 `darwin_amd64`、`darwin_arm64`、`linux_amd64` 和 `linux_arm64`。
`linux_amd64` 默认使用 `x86_64-unknown-linux-musl-gcc`、
`x86_64-unknown-linux-musl-ar` 和 `x86_64-unknown-linux-musl-ranlib`。

```sh
cd common/unigate
make
```

在 Apple Silicon macOS 上，构建结果例如：

```text
common/unigate/lib/darwin_arm64/libunigate.a
```

运行 C 冒烟测试：

```sh
make test
```

启用严格警告检查：

```sh
make clean
make test EXTRA_CFLAGS=-Werror
```

交叉编译 Linux amd64 静态库：

```sh
make \
  GOOS=linux \
  GOARCH=amd64
```

对应产物为 `lib/linux_amd64/libunigate.a`。Makefile 会自动选择上述 musl 工具链；
仍可通过命令行显式覆盖 `CC`、`AR` 和 `RANLIB`。Linux arm64 必须指定 aarch64
musl 工具链：

```sh
make \
  GOOS=linux \
  GOARCH=arm64 \
  CC=aarch64-linux-musl-gcc \
  AR=aarch64-linux-musl-ar \
  RANLIB=aarch64-linux-musl-ranlib
```

不同目标使用
`.build/<goos>_<goarch>/` 和 `lib/<goos>_<goarch>/`，不会复用其他平台的对象文件。
`make clean` 只清理当前目标；`make clean-all` 清理全部目标和静态库。
在 macOS 上可交叉编译 Linux 静态库，但 `make test` 还要求能在 Linux/musl 环境
执行测试程序。

## C 调用方法

### 编译和链接

```sh
TARGET="$(go env GOOS)_$(go env GOARCH)"
cc app.c \
  -I common/unigate \
  -L "common/unigate/lib/$TARGET" \
  -lunigate \
  -pthread \
  -o app
```

macOS 通常不需要显式添加 `-pthread`，Linux 必须添加。静态库已经包含 weblite、
mbedTLS 和 cJSON，不需要另外链接 OpenSSL、libcurl 或其他 SSL 动态库。

### 基本流程

1. 填写 `unigate_client_options`。
2. 填写 `unigate_request`，通过 `operation` 选择接口。
3. 调用 `unigate_execute_request`。
4. HTTP 响应或 Magensa fault 从 `unigate_response` 读取。
5. 调用 `unigate_response_free` 释放响应。

### EMV 授权示例

```c
#include <stdio.h>

#include "unigate.h"

static void log_message(const char *message, void *user_data) {
    (void)user_data;
    fprintf(stderr, "%s\n", message);
}

int main(void) {
    unigate_client_options options = {
        .base_url = NULL,  /* NULL 或空字符串使用 Magensa 默认地址 */
        .customer_code = "customer-code",
        .user_name = "user-name",
        .password = "password",
        .ca_path = NULL,   /* 也可以指定 PEM CA 文件 */
        .timeout_ms = 30000,
        .log_request_body = 1,
        .log_callback = log_message,
        .log_user_data = NULL,
    };
    unigate_request request = {
        .operation = UNIGATE_OP_EMV,
        .customer_transaction_id = "order-1",
        .transaction_type = "AUTHORIZE",
        .amount = 100.00,
        .data_type = "ARQC",
        .data = "<ARQC>",
        .payment_type = "Credit",
    };
    unigate_response response;

    int rc = unigate_execute_request(&options, &request, &response);
    if (rc != 0) {
        fprintf(stderr, "request failed: %s\n",
                response.error != NULL ? response.error : "unknown error");
        unigate_response_free(&response);
        return 1;
    }

    if (response.status_code >= 200 && response.status_code < 300) {
        if (response.body != NULL) {
            fwrite(response.body, 1, response.body_len, stdout);
            fputc('\n', stdout);
        }
    } else {
        fprintf(stderr, "Magensa %s %s failed: status=%d code=%s message=%s traceID=%s\n",
                response.method != NULL ? response.method : "",
                response.path != NULL ? response.path : "",
                response.status_code,
                response.fault_code != NULL ? response.fault_code : "",
                response.fault_message != NULL ? response.fault_message : "",
                response.fault_trace_id != NULL ? response.fault_trace_id : "");
    }

    unigate_response_free(&response);
    return 0;
}
```

`unigate_execute_request` 的返回值语义：

- `0`：已经收到 HTTP 响应，包括 4xx 和 5xx。
- `-1`：请求构建、网络、TLS、协议或内存分配失败，此时读取 `response.error`。
- HTTP 2xx 时，`response.body` 和 `response.body_len` 是 Magensa 返回的 JSON。
- HTTP 非 2xx 时，`response.fault_code`、`fault_message` 和 `fault_trace_id`
  是解析出的 Magensa fault 字段。

成功响应和 fault 响应都必须在最后调用 `unigate_response_free`。该函数会释放
`method`、`path`、`body`、fault 字段和 `error`。

### 操作类型

C 和 Go 使用同一组操作，字段内容也一一映射。

| C 枚举 | Go 常量 | HTTP 方法 | 接口 |
| --- | --- | --- | --- |
| `UNIGATE_OP_EMV` | `OperationEMV` | POST | `/api/Transaction/EMV` |
| `UNIGATE_OP_TOKEN` | `OperationToken` | POST | `/api/Transaction/Token` |
| `UNIGATE_OP_CARD_SWIPE` | `OperationCardSwipe` | POST | `/api/Transaction/CardSwipe` |
| `UNIGATE_OP_KEYPAD` | `OperationKeypad` | POST | `/api/Transaction/KeyPadEntry` |
| `UNIGATE_OP_MANUAL` | `OperationManual` | POST | `/api/Transaction/ManualEntry` |
| `UNIGATE_OP_INAPP_APPLE_PAY` | `OperationInAppApplePay` | POST | `/api/Transaction/InAppApplePay` |
| `UNIGATE_OP_TEC_APPLE_PAY` | `OperationTECApplePay` | POST | `/api/Transaction/TECApplePay` |
| `UNIGATE_OP_GOOGLE_PAY` | `OperationGooglePay` | POST | `/api/Transaction/GooglePay` |
| `UNIGATE_OP_INCREMENTAL_AUTHORIZE` | `OperationIncrementalAuthorize` | POST | `/api/ReferenceTransaction/AUTHORIZE` |
| `UNIGATE_OP_TIP_ADJUST` | `OperationTipAdjust` | POST | `/api/ReferenceTransaction/TIPADJUST` |
| `UNIGATE_OP_CAPTURE` | `OperationCapture` | POST | `/api/ReferenceTransaction/CAPTURE` |
| `UNIGATE_OP_REFUND` | `OperationRefund` | POST | `/api/ReferenceTransaction/REFUND` |
| `UNIGATE_OP_VOID` | `OperationVoid` | POST | `/api/ReferenceTransaction/VOID` |
| `UNIGATE_OP_GET_BY_MAG_TRAN_ID` | `OperationGetByMagTranID` | GET | `/api/Transaction/{magTranID}` |
| `UNIGATE_OP_GET_BY_CUSTOMER_TRAN_ID` | `OperationGetByCustomerTranID` | GET | `/api/Transaction?custTranID=...` |
| `UNIGATE_OP_APPLE_TAP_TO_PAY_READER` | `OperationAppleTapToPayReader` | POST | `/api/AppleTapToPayToken/PaymentCardReader` |

### 各操作需要的字段

#### EMV

- `customer_transaction_id`
- `transaction_type`
- `amount`
- `data_type`
- `data`
- `payment_type`

#### Token

- `customer_transaction_id`
- `transaction_type`
- `amount`
- `token`

#### CardSwipe

- `customer_transaction_id`
- `transaction_type`
- `amount`
- `track1`、`track2`、`track3`
- `magne_print`、`magne_print_status`
- `ksn`、`key_variant`
- `cvv`、`zip`
- `payment_type`
- `is_fallback`

#### KeyPadEntry

- `customer_transaction_id`
- `transaction_type`
- `amount`
- `track1`、`track2`、`track3`
- `ksn`、`key_variant`
- `cvv`、`zip`
- `payment_type`

#### ManualEntry

- `customer_transaction_id`
- `transaction_type`
- `amount`
- `address_line1`、`address_line2`
- `city`、`state`、`country`、`zip`
- `name_on_card`、`pan`
- `expiration_date`、`cvv`

#### Apple Pay 和 Google Pay

Apple Pay InApp：

- `customer_transaction_id`
- `transaction_type`
- `amount`
- `apple_pay_token`

Apple Pay TEC 使用相同字段，但 `operation` 改为 `UNIGATE_OP_TEC_APPLE_PAY`。

Google Pay：

- `customer_transaction_id`
- `transaction_type`
- `amount`
- `google_pay_token`

#### ReferenceTransaction

Incremental Authorize、Capture 和 Refund：

- `reference_mag_tran_id`
- `amount`

Tip Adjust：

- `reference_mag_tran_id`
- `tip_amount`
- `transaction_input_details` 和 `transaction_input_detail_count` 可选

Void：

- 全额撤销：只设置 `reference_mag_tran_id`
- 部分撤销：同时设置 `has_amount = 1` 和 `amount`

#### 查询

根据 Magensa 交易 ID 查询：

```c
unigate_request request = {
    .operation = UNIGATE_OP_GET_BY_MAG_TRAN_ID,
    .mag_tran_id = "mag-tran-id",
};
```

根据客户交易 ID 查询：

```c
unigate_request request = {
    .operation = UNIGATE_OP_GET_BY_CUSTOMER_TRAN_ID,
    .customer_transaction_id_query = "order-id",
};
```

查询参数由 C SDK 执行 URL 编码。

### CA 证书

TLS 默认校验证书链和主机名。CA 文件选择顺序：

1. `unigate_client_options.ca_path`
2. 环境变量 `UNIGATE_CA_BUNDLE`
3. 常见系统 CA 文件路径

嵌入式部署建议明确设置 `ca_path`，并保证设备时间正确，否则证书有效期校验会失败。

### 请求日志脱敏

设置 `log_request_body = 1` 并通过 `log_callback` 接收日志。SDK 会先脱敏再进行回调，
ARQC、磁道、KSN、PAN、CVV 等敏感值不会出现在日志中。

也可以单独调用脱敏函数：

```c
char *json = unigate_redact_json(body, body_len);
if (json != NULL) {
    puts(json);
    unigate_free(json);
}
```

## Go 调用方法

Go 包使用 cgo 链接预构建的当前目标 `lib/<goos>_<goarch>/libunigate.a`，
不会直接编译 C 源码。`binding.go` 中的 cgo LDFLAGS 按
`darwin/linux` 和 `amd64/arm64` 分别选择静态库。

### 构建前置条件

```sh
cd common/unigate
make
```

Go 构建需要：

```text
CGO_ENABLED=1
```

修改 C 代码后需要用与 Go 构建相同的 `GOOS`、`GOARCH` 重新执行 `make`，再构建
Go 程序。cgo 不支持跨架构复用静态库。

Linux amd64 的 Go 编译还必须让 cgo 使用同一套 musl 工具链：

```sh
CGO_ENABLED=1 \
GOOS=linux \
GOARCH=amd64 \
CC=x86_64-unknown-linux-musl-gcc \
go build ./your-service
```

Go 命令不会读取 Makefile 中的 `CC`，所以不能只依赖 `make` 的默认值。

`CGO_ENABLED=0` 时包仍可编译，但没有原生 C SDK：`Execute` 返回
`unigate: native C SDK requires ...` 错误，`RedactJSON` 返回
`<redact-unavailable>`，不会输出未经 C SDK 脱敏的原始报文。

### EMV 授权示例

```go
package main

import (
    "fmt"
    "log"
    "time"

    "uopenbox/common/unigate"
)

func main() {
    options := unigate.ClientOptions{
        CustomerCode:   "customer-code",
        UserName:       "user-name",
        Password:       "password",
        CAPath:         "/path/to/ca.pem", // 可为空
        Timeout:        30 * time.Second,  // 零值默认 30 秒
        LogRequestBody: false,
    }
    request := unigate.Request{
        Operation:             unigate.OperationEMV,
        CustomerTransactionID: "order-1",
        TransactionType:       "AUTHORIZE",
        Amount:                100,
        DataType:              "ARQC",
        Data:                  "<ARQC>",
        PaymentType:           "Credit",
    }

    response, err := unigate.Execute(options, request)
    if err != nil {
        log.Fatalf("request failed: %v", err)
    }
    if response.StatusCode < 200 || response.StatusCode >= 300 {
        log.Fatalf(
            "Magensa %s %s failed: status=%d code=%s message=%s traceID=%s",
            response.Method,
            response.Path,
            response.StatusCode,
            response.FaultCode,
            response.FaultMessage,
            response.FaultTraceID,
        )
    }

    fmt.Printf("status=%d body=%s\n", response.StatusCode, response.Body)
}
```

Go 的 `Execute` 返回值语义：

- `err != nil`：请求构建、网络、TLS、协议或内存分配失败。
- `err == nil` 且 `StatusCode` 为 2xx：`Body` 是 Magensa 成功响应 JSON。
- `err == nil` 但 `StatusCode` 非 2xx：读取 `FaultCode`、`FaultMessage` 和
  `FaultTraceID`。

### Capture 示例

```go
response, err := unigate.Execute(options, unigate.Request{
    Operation:          unigate.OperationCapture,
    ReferenceMagTranID: "mag-tran-id",
    Amount:             20.00,
})
```

### Tip Adjust 示例

```go
response, err := unigate.Execute(options, unigate.Request{
    Operation:          unigate.OperationTipAdjust,
    ReferenceMagTranID: "mag-tran-id",
    TipAmount:          5.00,
    TransactionInputDetails: []unigate.KeyValue{
        {Key: "tipType", Value: "percent"},
    },
})
```

### 部分 Void 示例

```go
response, err := unigate.Execute(options, unigate.Request{
    Operation:          unigate.OperationVoid,
    ReferenceMagTranID: "mag-tran-id",
    HasAmount:          true,
    Amount:             6.50,
})
```

全额 Void 不设置 `HasAmount`：

```go
response, err := unigate.Execute(options, unigate.Request{
    Operation:          unigate.OperationVoid,
    ReferenceMagTranID: "mag-tran-id",
})
```

### 查询示例

```go
byMagTranID, err := unigate.Execute(options, unigate.Request{
    Operation: unigate.OperationGetByMagTranID,
    MagTranID: "mag-tran-id",
})

byCustomerID, err := unigate.Execute(options, unigate.Request{
    Operation:                unigate.OperationGetByCustomerTranID,
    CustomerTransactionQuery: "order-id",
})
```

### Go 响应字段

| 字段 | 说明 |
| --- | --- |
| `StatusCode` | Magensa HTTP 状态码 |
| `Method` | C SDK 实际使用的 HTTP 方法 |
| `Path` | C SDK 生成的接口路径和查询参数 |
| `Body` | Magensa 原始响应 JSON |
| `FaultCode` | Magensa 错误码 |
| `FaultMessage` | Magensa 错误描述 |
| `FaultTraceID` | Magensa 排查用的 trace ID |

Go 绑定不会把成功响应解析成固定交易结构，调用方按 Magensa 返回的 JSON 自行解析。

## 运行测试

```sh
cd common/unigate
make clean
make test EXTRA_CFLAGS=-Werror

cd ../..
go test ./common/unigate
CGO_ENABLED=0 go test ./common/unigate
go test ./martbox-api/...
```

测试覆盖全部操作路径、请求 JSON、基础认证、HTTP fault、HTTPS/mbedTLS、日志脱敏和
C 静态库独立链接；非 cgo 构建覆盖明确的不可用边界。
