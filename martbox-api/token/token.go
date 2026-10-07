// Package token 负责签发和校验设备 token。
//
// 设备先用 imei 换一个 token，后续建 websocket 长连接时带上它证明身份。
// token 用 HS256 签发，密钥来自配置 Auth.Secret。
package token

import (
	"errors"
	"fmt"
	"time"

	"github.com/golang-jwt/jwt/v4"
)

// DefaultTTL 是未配置有效期时使用的默认值。
const DefaultTTL = 30 * 24 * time.Hour

// ErrEmptySecret 表示没有配置签名密钥。此时拒绝签发和校验，
// 避免用空密钥签出任何人都能伪造的 token。
var ErrEmptySecret = errors.New("token: 未配置签名密钥 Auth.Secret")

// Claims 是设备 token 的载荷。
type Claims struct {
	IMEI string `json:"imei"` // 设备 IMEI
	jwt.RegisteredClaims
}

// Signer 用固定密钥签发和校验 token。
type Signer struct {
	secret []byte
	issuer string
	ttl    time.Duration
}

// NewSigner 创建签发器。ttl <= 0 时使用 DefaultTTL。
func NewSigner(secret, issuer string, ttl time.Duration) *Signer {
	if ttl <= 0 {
		ttl = DefaultTTL
	}

	return &Signer{
		secret: []byte(secret),
		issuer: issuer,
		ttl:    ttl,
	}
}

// TTL 返回当前有效期。
func (s *Signer) TTL() time.Duration {
	return s.ttl
}

// Generate 为 imei 签发 token，同时返回载荷，方便调用方回传过期时间。
func (s *Signer) Generate(imei string) (string, *Claims, error) {
	if imei == "" {
		return "", nil, errors.New("token: imei 不能为空")
	}
	if len(s.secret) == 0 {
		return "", nil, ErrEmptySecret
	}

	now := time.Now()
	claims := &Claims{
		IMEI: imei,
		RegisteredClaims: jwt.RegisteredClaims{
			Issuer:    s.issuer,
			Subject:   imei,
			IssuedAt:  jwt.NewNumericDate(now),
			NotBefore: jwt.NewNumericDate(now),
			ExpiresAt: jwt.NewNumericDate(now.Add(s.ttl)),
		},
	}

	signed, err := jwt.NewWithClaims(jwt.SigningMethodHS256, claims).SignedString(s.secret)
	if err != nil {
		return "", nil, fmt.Errorf("token: 签发失败: %w", err)
	}

	return signed, claims, nil
}

// Parse 校验签名、有效期和签发人，并返回载荷。
func (s *Signer) Parse(tokenStr string) (*Claims, error) {
	if len(s.secret) == 0 {
		return nil, ErrEmptySecret
	}

	claims := &Claims{}
	parsed, err := jwt.ParseWithClaims(tokenStr, claims, func(*jwt.Token) (interface{}, error) {
		return s.secret, nil
	}, jwt.WithValidMethods([]string{jwt.SigningMethodHS256.Alg()}))
	if err != nil {
		return nil, fmt.Errorf("token: 校验失败: %w", err)
	}
	if !parsed.Valid {
		return nil, errors.New("token: 无效")
	}
	if s.issuer != "" && claims.Issuer != s.issuer {
		return nil, fmt.Errorf("token: 签发人不匹配: %s", claims.Issuer)
	}
	if claims.IMEI == "" {
		return nil, errors.New("token: 载荷缺少 imei")
	}

	return claims, nil
}
