package config

import "uopenbox/common/conf"

type OrderPayLink struct {
	NotifyUrl     string `yaml:"NotifyUrl"`
	ReturnUrl     string `yaml:"ReturnUrl"`
	JumpUrl       string `yaml:"JumpUrl"`
	CancelJumpUrl string `yaml:"CancelJumpUrl"`
}

type PcOrderPayLink struct {
	NotifyUrl     string `yaml:"NotifyUrl"`
	ReturnUrl     string `yaml:"ReturnUrl"`
	JumpUrl       string `yaml:"JumpUrl"`
	CancelJumpUrl string `yaml:"CancelJumpUrl"`
}

// WebSocketConf 是设备长连接的配置。所有时间字段单位为秒，数值为 0 时使用代码里的默认值。
type WebSocketConf struct {
	Path           string   `yaml:"Path"`           // 升级地址，默认 /v1/ws
	Token          string   `yaml:"Token"`          // 非空时，设备连接必须带 ?token= 且与之一致
	AllowedOrigins []string `yaml:"AllowedOrigins"` // 浏览器来源白名单；设备端不带 Origin，直接放行
	PingInterval   int      `yaml:"PingInterval"`   // 服务端心跳间隔
	PongTimeout    int      `yaml:"PongTimeout"`    // 心跳响应超时
	WriteTimeout   int      `yaml:"WriteTimeout"`   // 单次写超时
	ReadLimit      int64    `yaml:"ReadLimit"`      // 单条上行报文字节上限
	SendBuffer     int      `yaml:"SendBuffer"`     // 单连接发送队列长度
}

// AuthConf 是设备授权配置，用于签发 /author 返回的 token。
type AuthConf struct {
	Secret string `yaml:"Secret"` // JWT 签名密钥，不能为空
	Issuer string `yaml:"Issuer"` // 签发人，写入 iss 并参与校验
	Expire int    `yaml:"Expire"` // token 有效期，秒；为 0 时用代码默认值（30 天）
}

type Config struct {
	Mode string `yaml:"Mode"`

	Listen string `yaml:"Listen"`

	WebSocket WebSocketConf `yaml:"WebSocket"`

	Auth AuthConf `yaml:"Auth"`

	OrderPayLink `yaml:"OrderPayLink"`

	PcOrderPayLink `yaml:"PcOrderPayLink"`

	PhoneRecharegeRpc conf.RpcClientConf `yaml:"PhoneRecharegeRpc"`

	LayoutRpc conf.RpcClientConf `yaml:"LayoutRpc"`

	UserRpc conf.GoZeroClientConf `yaml:"UserRpc"`
}

var config Config

func InitConfig(configFile string) {
	err := conf.LoadConfig(configFile, &config)

	if err != nil {
		panic(err)
	}
}

func GetConfig() *Config {
	return &config
}
