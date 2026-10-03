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

设备先用 imei 换 token，token 用 HS256 签发，密钥来自配置 `Auth.Secret`。

```
POST /author
{"imei":"860000000000001","sn":"SN-0001"}
```

```json
{"token":"eyJhbGciOi...","imei":"860000000000001","expiresAt":1760000000,"expiresIn":2592000}
```

token 载荷带 `imei`（同时写入 `sub`）、`iss`、`iat`、`nbf`、`exp`。
校验用 `token.NewSigner(secret, issuer, ttl).Parse(tok)`，会校验签名、有效期和签发人，
并拒绝 `alg=none` 之类的算法混淆。

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
心跳由服务端发起 ping，`PongTimeout` 内没收到 pong 就断开并下线。
# uob_test
