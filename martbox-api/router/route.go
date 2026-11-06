package router

import (
	"errors"
	"net/http"
	"strings"
	"time"

	"uopenbox/martbox-api/config"
	v1 "uopenbox/martbox-api/handlers/v1"
	"uopenbox/martbox-api/token"
	"uopenbox/martbox-api/ws"

	"github.com/gin-gonic/gin"
)

func InitRouter(router *gin.Engine) {
	group := router.Group("/v1/transaction")
	group.POST("/emv", v1.Emv)         //支付
	group.POST("/capture", v1.Capture) //扣款

	router.POST("/author", v1.Author)
	router.GET("/v1/home", v1.Home) // APP 首页 banner + 商品
	initWebSocket(router)
}

// initWebSocket 初始化设备长连接：GET {Path} 建连（Authorization: Bearer），
// POST {Path}/push 服务端下发指令。
func initWebSocket(router *gin.Engine) {
	conf := config.GetConfig().WebSocket
	auth := config.GetConfig().Auth
	signer := token.NewSigner(auth.Secret, auth.Issuer, time.Duration(auth.Expire)*time.Second)

	hub := ws.Init(ws.Options{
		Authenticate: func(r *http.Request) (string, error) {
			return bearerDeviceSN(r, signer)
		},
		AllowedOrigins:   conf.AllowedOrigins,
		ReadLimit:        conf.ReadLimit,
		HeartbeatTimeout: time.Duration(conf.HeartbeatTimeout) * time.Second,
		WriteTimeout:     time.Duration(conf.WriteTimeout) * time.Second,
		SendBuffer:       conf.SendBuffer,
	})
	hub.OnMessage(v1.OnMachineMessage)
	hub.OnDisconnect(v1.OnMachineOffline)

	path := strings.TrimRight(conf.Path, "/")
	if path == "" {
		path = "/v1/ws"
	}

	router.GET(path, hub.Handle)
	router.POST(path+"/push", hub.HandlePush)
}

// bearerDeviceSN 从 Authorization: Bearer <token> 解析设备 token，并返回对应的 SN。
func bearerDeviceSN(r *http.Request, signer *token.Signer) (string, error) {
	parts := strings.SplitN(strings.TrimSpace(r.Header.Get("Authorization")), " ", 2)
	if len(parts) != 2 || !strings.EqualFold(parts[0], "Bearer") {
		return "", errors.New("缺少 Authorization: Bearer token")
	}

	claims, err := signer.Parse(strings.TrimSpace(parts[1]))
	if err != nil {
		return "", err
	}

	return token.DeviceSN(claims.IMEI), nil
}
