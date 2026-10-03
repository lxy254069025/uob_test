package router

import (
	"time"

	"uopenbox/martbox-api/config"
	v1 "uopenbox/martbox-api/handlers/v1"
	"uopenbox/martbox-api/ws"

	"github.com/gin-gonic/gin"
)

func InitRouter(router *gin.Engine) {
	group := router.Group("/v1/transaction")
	group.POST("/emv", v1.Emv)

	router.POST("/author", v1.Author)
	initWebSocket(router)
}

// initWebSocket 初始化设备长连接：GET {Path}/machine 建连，POST {Path}/push 服务端下发指令。
func initWebSocket(router *gin.Engine) {
	conf := config.GetConfig().WebSocket

	hub := ws.Init(ws.Options{
		Token:          conf.Token,
		AllowedOrigins: conf.AllowedOrigins,
		ReadLimit:      conf.ReadLimit,
		PingInterval:   time.Duration(conf.PingInterval) * time.Second,
		PongTimeout:    time.Duration(conf.PongTimeout) * time.Second,
		WriteTimeout:   time.Duration(conf.WriteTimeout) * time.Second,
		SendBuffer:     conf.SendBuffer,
	})
	hub.OnMessage(v1.OnMachineMessage)

	path := conf.Path
	if path == "" {
		path = "/v1/ws"
	}

	group := router.Group(path)
	group.GET("/machine", hub.Handle)
	group.POST("/push", hub.HandlePush)
}
