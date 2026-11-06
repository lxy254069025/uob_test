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
	Token     string `json:"token"`     // 后续 HTTP / WebSocket 请求都带上
	SN        string `json:"sn"`        // 设备号，WebSocket 上行报文用它标识设备
	ExpiresAt int64  `json:"expiresAt"` // 过期时间，秒级时间戳
	ExpiresIn int64  `json:"expiresIn"` // 剩余有效秒数
}

// Author 授权：根据 imei 生成 token。
//
// 当前不校验 imei 是否是已登记的设备，只要格式非空就能换到 token。
// 等 machines-rpc 能查设备表之后，这里应该补一层校验：imei 必须存在且状态正常，
// 否则任何知道 imei 的人都能拿到 token。
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
		SN:        token.DeviceSN(imei),
		ExpiresAt: claims.ExpiresAt.Unix(),
		ExpiresIn: int64(time.Until(claims.ExpiresAt.Time).Seconds()),
	})
}
