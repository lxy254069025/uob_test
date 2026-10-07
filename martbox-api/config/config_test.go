package config

import (
	"testing"

	"uopenbox/common/conf"
)

// TestHomeYamlLoads 防止 home.yaml 写错导致服务启动直接 panic。
func TestHomeYamlLoads(t *testing.T) {
	var cfg Config
	if err := conf.LoadConfig("../etc/home.yaml", &cfg); err != nil {
		t.Fatalf("配置文件加载失败: %v", err)
	}

	if cfg.Listen == "" {
		t.Fatal("Listen 不能为空")
	}
	if cfg.WebSocket.Path != "/v1/ws" {
		t.Fatalf("WebSocket.Path 期望 /v1/ws，实际 %q", cfg.WebSocket.Path)
	}
	if cfg.WebSocket.PingInterval != 30 || cfg.WebSocket.PongTimeout != 90 {
		t.Fatalf("WebSocket 心跳配置不符合预期: %+v", cfg.WebSocket)
	}
	if cfg.WebSocket.ReadLimit != 16384 || cfg.WebSocket.SendBuffer != 64 {
		t.Fatalf("WebSocket 缓冲配置不符合预期: %+v", cfg.WebSocket)
	}
	if len(cfg.WebSocket.AllowedOrigins) != 0 {
		t.Fatalf("默认不放开浏览器来源，实际 %v", cfg.WebSocket.AllowedOrigins)
	}

	if cfg.Auth.Secret == "" {
		t.Fatal("Auth.Secret 不能为空，否则签发 token 会被拒绝")
	}
	if cfg.Auth.Issuer == "" {
		t.Fatal("Auth.Issuer 不能为空")
	}
	if cfg.Auth.Expire <= 0 {
		t.Fatalf("Auth.Expire 应为正数，实际 %d", cfg.Auth.Expire)
	}

	// Unigate 段能被正确解析即可；"默认关闭"是代码里的行为保证，
	// 由 common/unigate 的 TestRequestBodyLoggingDisabledByDefault 守着，
	// 这里不该把运维的开关值写死。
}
