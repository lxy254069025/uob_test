```
.
├── common // common 
│   ├── conf
│   │   └── config.go
│   ├── go.mod
│   ├── go.sum
│   └── unigate     //magtek SDK
│       ├── appletaptopay.go
│       ├── client.go
│       ├── redact.go
│       ├── reference_transaction.go
│       ├── transaction.go
│       └── types.go
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

{"imei":"860000000000001","sn":"SN-0001"}
```

```json
{"token":"eyJhbGciOi...","imei":"860000000000001","expiresAt":1760000000,"expiresIn":2592000}
```

token 用 HS256 签发，密钥来自配置 `Auth.Secret`，有效期 `Auth.Expire`（默认 30 天）。
载荷带 `imei`（同时写入 `sub`）、`iss`、`iat`、`nbf`、`exp`。
校验用 `token.NewSigner(secret, issuer, ttl).Parse(tok)`，会校验签名、有效期和签发人，
并拒绝 `alg=none` 之类的算法混淆。

imei 为空或请求体不是合法 JSON 返回 400；签发失败（例如没配密钥）返回 500。

⚠️ 当前不校验 imei 是否合法、是否是已登记的设备，**只要非空就能换到 token**。
等 machines-rpc 能查设备表后，应在 `Author` 里补一层"imei 必须存在且状态正常"的校验。
另外 access_token 目前还没有接到 websocket 建连校验上。

## Magensa 交易接口

`POST /v1/transaction/emv`（授权）和 `POST /v1/transaction/capture`（追加授权 + 扣款）
调 Magensa Unigate。上游返回非 2xx 时，`unigate.APIError` 会解析 Magensa 的 fault 结构，
把 `code` / `message` / `traceID` 单独带出来，不再是整坨 JSON：

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

建连：`GET /v1/ws/machine?sn=<设备序列号>[&token=<配置里的 Token>]`

报文格式统一为：

```json
{"type":"command","sn":"SN-0001","id":"msg-1","data":{"action":"drop"},"ts":1759000000000}
```

`type` 取值：`register`（设备注册，可在这条报文里补 sn）、`event`（设备事件上报）、
`ack`（指令回执，用 `id` 关联原消息）、`command`（服务端下发指令）、`error`（服务端报错）。

服务端下发指令除了在业务代码里调用 `ws.Default().Send(sn, msg)`，也可以走 HTTP：

```
POST /v1/ws/push
{"sn":"SN-0001","type":"command","id":"msg-1","data":{"action":"drop"}}
```

握手行为：设备端（不带 `Origin`）直接放行；浏览器来源必须在 `WebSocket.AllowedOrigins` 白名单里。

心跳与断线：**心跳由客户端发起**，设备定时发 ping，服务端收到后回同样的 pong；
超过 `WebSocket.HeartbeatTimeout` 秒没收到客户端任何数据（心跳或业务报文）就判定掉线，
断开连接、从在线列表移除，并触发断线处理 `hub.OnDisconnect`（目前接到
`v1.OnMachineOffline`，后续可在这里把设备标记为离线）。
超时原因会写进日志：

```
ws: 设备断线 sn=SN-0001 addr=1.2.3.4:5678 原因=心跳超时（1m30s 内未收到客户端心跳）
```

客户端如果习惯用 pong 而不是 ping 做心跳也能用——服务端把 pong 同样当作存活信号。
# uob_test
