package v1

import (
	"log"
	"net/http"
	"strings"
	"time"

	"uopenbox/martbox-api/config"
	"uopenbox/martbox-api/token"

	"github.com/gin-gonic/gin"
)

// AuthorRequest 设备授权请求：设备用 imei 换 token。
type AuthorRequest struct {
	IMEI string `json:"imei" binding:"required"` // 设备 IMEI
	SN   string `json:"sn"`                      // 可选，设备序列号，便于排查
}

// AuthorResponse 设备授权响应。
type AuthorResponse struct {
	Token     string `json:"token"`     // 后续建 websocket 长连接时带上
	IMEI      string `json:"imei"`      // 原样回显，便于设备端校对
	ExpiresAt int64  `json:"expiresAt"` // 过期时间，秒级时间戳
	ExpiresIn int64  `json:"expiresIn"` // 剩余有效秒数
}

// Author 授权：根据 imei 生成 token。
func Author(c *gin.Context) {
	var req AuthorRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "参数错误: " + err.Error()})
		return
	}

	imei := strings.TrimSpace(req.IMEI)
	if imei == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "imei 不能为空"})
		return
	}

	auth := config.GetConfig().Auth
	signer := token.NewSigner(auth.Secret, auth.Issuer, time.Duration(auth.Expire)*time.Second)

	signed, claims, err := signer.Generate(imei)
	if err != nil {
		// 密钥没配置等属于服务端问题，不要向设备暴露细节。
		log.Printf("author: imei=%s 生成 token 失败: %v", imei, err)
		c.JSON(http.StatusInternalServerError, gin.H{"error": "生成 token 失败"})
		return
	}

	c.JSON(http.StatusOK, AuthorResponse{
		Token:     signed,
		IMEI:      imei,
		ExpiresAt: claims.ExpiresAt.Unix(),
		ExpiresIn: int64(time.Until(claims.ExpiresAt.Time).Seconds()),
	})
}
