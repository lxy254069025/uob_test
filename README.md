```
.
├── common // common 
│   ├── conf
│   │   └── config.go
│   ├── go.mod
│   ├── go.sum
│   └── unigate     // Magensa C SDK + Go FFI
│       ├── types.go            // C/Go 共用的公开类型
│       ├── binding.go          // 当前 GOOS/GOARCH 的 cgo 绑定
│       ├── binding_nocgo.go    // CGO_ENABLED=0 的明确错误实现
│       ├── binding_test.go
│       ├── unigate.h           // 可移植 C API
│       ├── Makefile            // 按 GOOS/GOARCH 生成静态库
│       ├── c/                  // Unigate C 实现
│       ├── weblite/            // HTTP 源码
│       ├── mbedtls/            // TLS 源码和许可证
│       ├── cjson/              // JSON 源码和许可证
│       └── lib/<goos>_<goarch>/
│           └── libunigate.a
├── go.work
├── go.work.sum
├── martbox-api     // 自动售卖机接口
│   ├── config
│   │   └── config.go
│   ├── etc
│   │   └── home.yaml
│   ├── go.mod
│   ├── go.sum
│   ├── handlers
│   │   └── v1
│   │       ├── author.go       // 设备授权，imei 换 token
│   │       ├── home.go         // APP 首页 banner + 商品
│   │       ├── machine_ws.go   // 设备上行报文处理入口
│   │       └── transaction.go
│   ├── main.go
│   ├── middleware
│   │   └── check_token.go
│   ├── router
│   │   └── route.go
│   ├── token       // 设备 token 签发与校验
│   │   ├── token.go
│   │   └── token_test.go
│   └── ws          // 设备 websocket 长连接
│       ├── client.go
│       ├── handler.go
│       ├── hub.go
│       ├── hub_test.go
│       └── message.go
├── README.md
└── services        // 衔接API和RPC服务的SERVICE
    └── go.mod
    ```

## martbox-api WebSocket

### 设备授权

设备用 imei 换 token：

```
POST /author
Content-Type: application/json

{"imei":"860000000000001"}
```

```json
{"token":"eyJhbGciOi...","sn":"martbox-000001","expiresAt":1760000000,"expiresIn":2592000}
```

`sn` 是设备号，当前由 IMEI 后 6 位生成（`martbox-xxxxxx`）；APP 保存它，并在
WebSocket 上行报文里作为设备标识。接入 machines-rpc 设备表后应改为按 IMEI 查真实 SN。

token 用 HS256 签发，密钥来自配置 `Auth.Secret`，有效期 `Auth.Expire`（默认 30 天）。
载荷带 `imei`（同时写入 `sub`）、`iss`、`iat`、`nbf`、`exp`。
校验用 `token.NewSigner(secret, issuer, ttl).Parse(tok)`，会校验签名、有效期和签发人，
并拒绝 `alg=none` 之类的算法混淆。

imei 为空或请求体不是合法 JSON 返回 400；签发失败（例如没配密钥）返回 500。

⚠️ 当前不校验 imei 是否合法、是否是已登记的设备，**只要非空就能换到 token**。
等 machines-rpc 能查设备表后，应在 `Author` 里补一层"imei 必须存在且状态正常"的校验。
WebSocket 建连已经用这个 token 校验（`Authorization: Bearer <token>`）。

## APP 首页数据

APP 首屏通过一个接口同时获取 banner 和商品：

```
GET /v1/home
```

字段名与 APP 端 Kotlin 的 `Banner`、`Product` 数据类一致；`badgeText`、
`originalPrice` 可以为空，服务端会显式输出 `null`，不省略字段。
`off` 是整数优惠百分比（`12` 表示 12% off），客户端自己拼 `off` 后缀；
没有优惠时为 `0`。

```json
{
  "banners": [
    {
      "id": 1,
      "title": "新客专享",
      "subtitle": "首单立减 5 元",
      "ctaText": "立即领取",
      "badgeText": "新人",
      "url": "https://example.com/activities/new-user",
      "imageUrl": "https://picsum.photos/seed/martbox-new-user/1200/480"
    }
  ],
  "products": [
    {
      "name": "冰镇可乐 330ml",
      "price": "3.50",
      "originalPrice": "4.00",
      "off": 12,
      "desc": "经典口味，冰镇后更好喝",
      "imageUrl": "https://pngimg.com/uploads/cocacola/coca_cola_PNG8909.png"
    },
    {
      "name": "现磨美式咖啡",
      "price": "12.00",
      "originalPrice": null,
      "off": 0,
      "desc": "中深烘焙，现点现做",
      "imageUrl": "https://pngimg.com/uploads/mug_coffee/mug_coffee_PNG97402.png"
    }
  ]
}
```

当前数据写死在 `martbox-api/handlers/v1/home.go`：接口返回 3 条 banner 和 16 条商品，
商品由 3 条基础数据循环填充，方便联调 4x4 宫格。接入运营后台后，需要按设备所属
门店/城市过滤，并补上缓存。

## Magensa 交易接口

### C 实现与嵌入式依赖

`common/unigate` 是一个可独立使用的 C SDK。所有 Unigate 操作、接口路径、查询参数、
请求 JSON、HTTP/TLS、响应读取、fault 解析和敏感字段脱敏都由 C 实现。Go 文件只负责
cgo 结构映射，原有的 `Client`、交易请求类型和交易方法已经删除。

完整字段说明、全部操作和 C/Go 示例见
[common/unigate/README.md](common/unigate/README.md)。

依赖全部以源码形式随项目编译，不链接系统 OpenSSL、libcurl 或其他本机 SSL：

- HTTP/1.1：[weblite](common/unigate/weblite/weblite.c)
- TLS：[mbedTLS 3.6.4](common/unigate/mbedtls/LICENSE)，仅启用 TLS 1.2
  客户端、X.509、ECDHE/RSA、AES-GCM/CCM 和 ChaCha20-Poly1305
- JSON：[cJSON 1.7.18](common/unigate/cjson/LICENSE)
- 第三方版本、用途和许可证：[THIRD_PARTY_NOTICES.md](common/unigate/THIRD_PARTY_NOTICES.md)

构建静态库：

```sh
cd common/unigate
make
make test
```

构建产物按目标隔离，例如 `common/unigate/lib/darwin_arm64/libunigate.a`，
公开头文件是 `common/unigate/unigate.h`。其他 C/C++、Rust、Swift 等语言可以链接
对应目标的静态库；Linux 链接参数需要包含 `-pthread`。Linux amd64 默认使用
`x86_64-unknown-linux-musl-gcc` 及其对应的 `ar`、`ranlib`：

```sh
make GOOS=linux GOARCH=amd64
```

C 调用示例：

```c
unigate_client_options options = {
    .customer_code = "customer",
    .user_name = "user",
    .password = "password",
    .timeout_ms = 30000,
};
unigate_request request = {
    .operation = UNIGATE_OP_EMV,
    .customer_transaction_id = "order-1",
    .transaction_type = "AUTHORIZE",
    .amount = 100,
    .data_type = "ARQC",
    .data = "<ARQC>",
    .payment_type = "Credit",
};
unigate_response response;
int rc = unigate_execute_request(&options, &request, &response);
/* rc == 0 时 response.body 是 Magensa 返回的 JSON。 */
unigate_response_free(&response);
```

Go 包通过 cgo 链接当前 `GOOS/GOARCH` 对应的 `libunigate.a`，不会再直接编译 C 源码。
因此首次构建或修改 C 代码后，先用相同的 `GOOS/GOARCH` 执行 `make`，再执行 Go 构建
或测试。静态库支持 `darwin/linux` 的 `amd64/arm64`。

TLS 默认校验证书和主机名。可用 `unigate.ClientOptions.CAPath` 指定 CA 证书包，或设置
`UNIGATE_CA_BUNDLE`；未指定时会尝试常见系统证书路径。嵌入式部署建议随固件明确
配置 CA 文件，并确保系统时钟正确，否则证书有效期校验会失败。

Go 侧交易调用需要 `CGO_ENABLED=1`。`CGO_ENABLED=0` 时包可以编译，但 `Execute`
会返回明确的不可用错误。运行时不需要额外安装 HTTP、JSON 或 SSL 动态库；Linux
构建只需要 libc 和 pthread。

Go 不会读取 Makefile 的 `CC`。构建 Linux amd64 服务时必须同时指定 musl 编译器：

```sh
make -C martbox-api build-linux-amd64
```

该目标先生成 `common/unigate/lib/linux_amd64/libunigate.a`，再以
`CGO_ENABLED=1` 和 `x86_64-unknown-linux-musl-gcc` 生成完全静态的
`martbox-api/bin/martbox-api-linux-amd64`，构建结束会校验二进制元数据中的
`CGO_ENABLED=1`。

如果服务器没有系统 CA 包，需要安装 `ca-certificates`，或通过
`UNIGATE_CA_BUNDLE` 指向随部署文件提供的 PEM CA 包。

`POST /v1/transaction/emv`（授权）和 `POST /v1/transaction/capture`（追加授权 + 扣款）
调 Magensa Unigate。上游返回非 2xx 时，C SDK 会解析 Magensa 的 fault 结构，
并在 `unigate_response` / `unigate.Response` 中提供 `code` / `message` / `traceID`：

```
POST /v1/transaction/emv
{"paymentType":"Credit","data":"<卡片数据>"}
```

入参只保留设备真正知道的两项：`paymentType`（留空按 `Credit`）和 `data`（卡片数据）。
其余字段由**接口方法自己组装成自己的提交格式**，`/v1/transaction/emv` 这个方法固定用
`dataType=ARQC` + `transactionType=AUTHORIZE` + 方法自己的金额常量
（见 `emvAuthorizeRequest`）。换一种支付方式（iPhone Tap to Pay、刷卡、手工输入……）
应该是另一个方法、另一套组装逻辑，而不是靠调用方传 `dataType` 来切换——
之前正是"把苹果支付的载荷贴上 ARQC 标签"导致 Magensa 只回一个含糊的 UnknownError。

⚠️ `data` 的具体格式由方法决定：`/v1/transaction/emv` 要求 ARQC（读卡器产出的裸 EMV 数据）。
iPhone Tap to Pay 的载荷（Magensa 的 `AppleTapToPay` 容器）走不了这个方法，需要单独加一个方法。

重放交易：Magensa 认出同一段 ARQC 被重复提交时，会直接返回上一次的授权结果并把
`dataOutput.isReplay` 置为 `true`——**这笔并没有真的发起新交易**。接口会打告警日志，
设备端如果照常出货就会出现"出两次货、只扣一次钱"，所以最好不要复用 ARQC。

```
unigate: POST /api/Transaction/EMV failed: InvalidPaymentMode: PaymentMode is not supported - Invalid value [] for tag DFDF52 in ARQC (status 400, traceID c1d35cdb-...)
```

对设备的响应：上游 4xx 说明送上去的数据有问题，转成 400；上游 5xx 转成 502；
响应体里带 `error` / `message` / `traceID`，设备报障时把 traceID 给 Magensa 即可定位。
handler 里不再用 `log.Fatal`——一次上游失败曾经会直接杀掉整个 API 进程。
错误信息里带请求路径，是因为 `/capture` 会连调 `IncrementalAuthorize` 和 `Capture` 两次，
不带路径就看不出是哪一步挂的。

排查请求体（比如确认 ARQC 是不是空的或被截断）时，把 `Unigate.LogRequestBody` 打开：

```
unigate: POST /api/Transaction/EMV body={"customerTransactionID":"order-1","dataInput":{"encryptedData":{"data":"<redacted len=36 sha256=1a2b3c4d5e6f>","dataType":"ARQC"},"paymentType":"Credit"},"transactionInput":{"amount":100,"transactionType":"AUTHORIZE"}}
```

ARQC、磁道、KSN、卡号、CVV 这些字段只打印长度和截断的 SHA-256 指纹：长度判断有没有被截断，
指纹判断内容有没有被破坏（例如 base64 的 `+` 变成空格，长度不变但内容全坏）。
同样的内容指纹稳定，可以拿设备端的值和日志比对。空值原样保留，一眼能看出是没传值还是被截断。

设备（自动售卖机）通过 websocket 保持长连接，服务端可随时下发指令。

建连：

```
GET /v1/ws
Authorization: Bearer <token>
```

握手时服务端校验 token，并按 token 里的 imei 推导 sn；不再使用 `?sn` / `?token`
查询参数。APP 还没拿到 token 之前不会连接。

报文格式统一为：

```json
{"type":"command","sn":"martbox-000001","id":"msg-1","data":{"action":"drop"},"ts":1759000000000}
```

`type` 取值：`status`（定时上报状态，兼作应用层心跳）、`temperature`
（`get_temperature` 的回复）、`device_event`（设备事件上报）、`ack`（指令回执，用
`id` 关联原消息）、`command`（服务端下发指令）、`error`（服务端报错）。
未知类型只记日志，不影响连接。

设备定时上报的状态报文长这样（服务端按 `sn` 保存最近一次，供后续查询/落库）：

```json
{"type":"status","sn":"martbox-000001","ts":1728000000000,
 "data":{"battery":80,"volume":5,"networkType":"wifi","temperature":"","hardwareAvailable":true,
         "devices":[{"module":"lock","online":true,"busy":false,"doorOpen":false,"lockOpen":false},
                    {"module":"camera","online":true,"busy":false},
                    {"module":"pos","online":true,"busy":false},
                    {"module":"temperature","online":false,"busy":false}]}}
```

`doorOpen` / `lockOpen` 只有锁模块才有，所以内部用指针区分"没上报"和"明确为 false"。
`temperature` 用 `StringOrNumber` 接收：样例给的是空字符串，但设备直接发数字（`26.5`）
也能解析——否则类型不匹配会让**整份状态**解析失败。
模块有离线的会单独打一条告警日志。

服务端下发指令除了在业务代码里调用 `ws.Default().Send(sn, msg)`，也可以走 HTTP：

```
POST /v1/ws/push
{"sn":"martbox-000001","type":"command","id":"msg-1","data":{"action":"drop"}}
```

握手行为：设备端（不带 `Origin`）直接放行；浏览器来源必须在 `WebSocket.AllowedOrigins` 白名单里。
没有 `Authorization: Bearer <token>` 或 token 校验失败时返回 401，不会升级连接。

心跳与断线：**心跳是应用层的 status 报文**，设备定时上报状态，服务端收到就知道它还活着；
超过 `WebSocket.HeartbeatTimeout` 秒没收到客户端任何数据（status 或其它业务报文）就判定掉线，
断开连接、从在线列表移除，并触发断线处理 `hub.OnDisconnect`（目前接到
`v1.OnMachineOffline`，后续可在这里把设备标记为离线）。
超时原因会写进日志：

```
ws: 设备断线 sn=martbox-000001 addr=1.2.3.4:5678 原因=心跳超时（1m30s 内未收到客户端心跳）
```

协议层的 ping/pong 仍然兼容（收到 ping 照旧回 pong，收到 pong 也算存活），
所以客户端用哪种方式探活都不会被误判掉线。
# uob_test
