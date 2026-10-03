package token

import (
	"errors"
	"strings"
	"testing"
	"time"

	"github.com/golang-jwt/jwt/v4"
)

const (
	testSecret = "test-secret"
	testIssuer = "martbox-api"
	testIMEI   = "860000000000001"
)

func TestGenerateAndParse(t *testing.T) {
	signer := NewSigner(testSecret, testIssuer, time.Hour)

	signed, claims, err := signer.Generate(testIMEI)
	if err != nil {
		t.Fatalf("Generate 失败: %v", err)
	}
	if signed == "" {
		t.Fatal("签发结果不应为空")
	}
	if claims.IMEI != testIMEI {
		t.Fatalf("载荷 imei 期望 %s，实际 %s", testIMEI, claims.IMEI)
	}

	if want := time.Now().Add(time.Hour); claims.ExpiresAt.Time.Sub(want).Abs() > time.Minute {
		t.Fatalf("过期时间偏离预期: %v", claims.ExpiresAt.Time)
	}

	got, err := signer.Parse(signed)
	if err != nil {
		t.Fatalf("Parse 失败: %v", err)
	}
	if got.IMEI != testIMEI {
		t.Fatalf("解析出的 imei 期望 %s，实际 %s", testIMEI, got.IMEI)
	}
	if got.Issuer != testIssuer {
		t.Fatalf("解析出的 issuer 期望 %s，实际 %s", testIssuer, got.Issuer)
	}
	if got.Subject != testIMEI {
		t.Fatalf("sub 期望 %s，实际 %s", testIMEI, got.Subject)
	}
}

// 同一个 imei 每次签发的 token 都应该不同（iat/exp 有纳秒级差异），避免设备端缓存复用。
func TestGenerateProducesDifferentTokens(t *testing.T) {
	signer := NewSigner(testSecret, testIssuer, time.Hour)

	first, _, err := signer.Generate(testIMEI)
	if err != nil {
		t.Fatalf("Generate 失败: %v", err)
	}
	time.Sleep(1100 * time.Millisecond) // iat/exp 精度是秒
	second, _, err := signer.Generate(testIMEI)
	if err != nil {
		t.Fatalf("Generate 失败: %v", err)
	}

	if first == second {
		t.Fatal("两次签发的 token 不应完全相同")
	}
}

func TestParseRejectsWrongSecret(t *testing.T) {
	signer := NewSigner(testSecret, testIssuer, time.Hour)
	signed, _, err := signer.Generate(testIMEI)
	if err != nil {
		t.Fatalf("Generate 失败: %v", err)
	}

	other := NewSigner("another-secret", testIssuer, time.Hour)
	if _, err := other.Parse(signed); !errors.Is(err, jwt.ErrTokenSignatureInvalid) {
		t.Fatalf("换密钥校验应因签名无效失败，实际 %v", err)
	}
}

func TestParseRejectsTamperedToken(t *testing.T) {
	signer := NewSigner(testSecret, testIssuer, time.Hour)
	signed, _, err := signer.Generate(testIMEI)
	if err != nil {
		t.Fatalf("Generate 失败: %v", err)
	}

	// 改动载荷部分，签名就不再匹配。
	parts := strings.Split(signed, ".")
	if len(parts) != 3 {
		t.Fatalf("token 格式异常: %s", signed)
	}
	tampered := parts[0] + "." + parts[1][:len(parts[1])-1] + "A." + parts[2]

	if _, err := signer.Parse(tampered); err == nil {
		t.Fatal("被篡改的 token 不应校验通过")
	}
}

func TestParseRejectsExpiredToken(t *testing.T) {
	signer := NewSigner(testSecret, testIssuer, time.Hour)

	// 直接构造一个已过期的 token，避免依赖 sleep。
	expired := &Claims{
		IMEI: testIMEI,
		RegisteredClaims: jwt.RegisteredClaims{
			Issuer:    testIssuer,
			Subject:   testIMEI,
			IssuedAt:  jwt.NewNumericDate(time.Now().Add(-2 * time.Hour)),
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(-time.Hour)),
		},
	}
	signed, err := jwt.NewWithClaims(jwt.SigningMethodHS256, expired).SignedString([]byte(testSecret))
	if err != nil {
		t.Fatalf("构造过期 token 失败: %v", err)
	}

	if _, err := signer.Parse(signed); !errors.Is(err, jwt.ErrTokenExpired) {
		t.Fatalf("过期 token 应校验失败，实际 %v", err)
	}
}

// 防止 alg=none 之类的算法混淆攻击。
func TestParseRejectsNoneAlgorithm(t *testing.T) {
	signer := NewSigner(testSecret, testIssuer, time.Hour)

	claims := &Claims{
		IMEI: testIMEI,
		RegisteredClaims: jwt.RegisteredClaims{
			Issuer:    testIssuer,
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(time.Hour)),
		},
	}
	unsigned, err := jwt.NewWithClaims(jwt.SigningMethodNone, claims).SignedString(jwt.UnsafeAllowNoneSignatureType)
	if err != nil {
		t.Fatalf("构造 none 算法 token 失败: %v", err)
	}

	if _, err := signer.Parse(unsigned); err == nil {
		t.Fatal("alg=none 的 token 不应校验通过")
	}
}

func TestParseRejectsWrongIssuer(t *testing.T) {
	signer := NewSigner(testSecret, testIssuer, time.Hour)
	signed, _, err := signer.Generate(testIMEI)
	if err != nil {
		t.Fatalf("Generate 失败: %v", err)
	}

	other := NewSigner(testSecret, "another-issuer", time.Hour)
	if _, err := other.Parse(signed); err == nil {
		t.Fatal("签发人不匹配时不应校验通过")
	}
}

func TestEmptySecretIsRejected(t *testing.T) {
	signer := NewSigner("", testIssuer, time.Hour)

	if _, _, err := signer.Generate(testIMEI); !errors.Is(err, ErrEmptySecret) {
		t.Fatalf("空密钥签发应返回 ErrEmptySecret，实际 %v", err)
	}
	if _, err := signer.Parse("whatever"); !errors.Is(err, ErrEmptySecret) {
		t.Fatalf("空密钥校验应返回 ErrEmptySecret，实际 %v", err)
	}
}

func TestEmptyIMEIIsRejected(t *testing.T) {
	signer := NewSigner(testSecret, testIssuer, time.Hour)

	if _, _, err := signer.Generate(""); err == nil {
		t.Fatal("空 imei 不应签发成功")
	}
}

func TestNewSignerUsesDefaultTTL(t *testing.T) {
	for _, ttl := range []time.Duration{0, -time.Hour} {
		if got := NewSigner(testSecret, testIssuer, ttl).TTL(); got != DefaultTTL {
			t.Fatalf("ttl=%v 应回落到默认值 %v，实际 %v", ttl, DefaultTTL, got)
		}
	}

	if got := NewSigner(testSecret, testIssuer, time.Minute).TTL(); got != time.Minute {
		t.Fatalf("显式 ttl 不应被覆盖，实际 %v", got)
	}
}
