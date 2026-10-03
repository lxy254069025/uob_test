package ws

import (
	"net/http"
	"strings"

	"github.com/gin-gonic/gin"
)

// Handle 是 Gin 的升级入口：GET /v1/ws/machine?sn=xxx[&token=yyy]。
// 升级成功后连接交给 Hub 管理，函数返回时连接仍然存活。
func (h *Hub) Handle(c *gin.Context) {
	sn := strings.TrimSpace(c.Query("sn"))
	if sn == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "缺少设备序列号 sn"})
		return
	}

	if h.opts.Token != "" && c.Query("token") != h.opts.Token {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "token 校验失败"})
		return
	}

	conn, err := h.upgrader.Upgrade(c.Writer, c.Request, nil)
	if err != nil {
		// 升级失败时 Upgrade 已经写过响应，这里只把错误挂到 gin 上下文，便于日志中间件记录。
		_ = c.Error(err)
		return
	}

	client := newClient(h, conn, sn)
	h.register(client)

	go client.writePump()
	go client.readPump()
}

// checkOrigin 约束浏览器来源；设备端（非浏览器）请求不带 Origin，直接放行。
func (h *Hub) checkOrigin(r *http.Request) bool {
	origin := r.Header.Get("Origin")
	if origin == "" {
		return true
	}

	for _, allowed := range h.opts.AllowedOrigins {
		if allowed == "*" || strings.EqualFold(allowed, origin) {
			return true
		}
	}

	return false
}

// PushRequest 是服务端主动下发指令的请求体。
type PushRequest struct {
	SN   string      `json:"sn" binding:"required"` // 目标设备序列号
	Type string      `json:"type"`                  // 报文类型，默认 command
	ID   string      `json:"id"`                    // 消息 ID，设备回 ack 时带回
	Data interface{} `json:"data"`                  // 业务数据
}

// HandlePush 提供 HTTP 方式给设备下发指令，方便联调和后台触发。
func (h *Hub) HandlePush(c *gin.Context) {
	var req PushRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	typ := req.Type
	if typ == "" {
		typ = TypeCommand
	}

	msg, err := NewMessage(typ, req.SN, req.ID, req.Data)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	sent, err := h.Send(req.SN, msg)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"sn":        req.SN,
		"online":    sent > 0,
		"delivered": sent,
	})
}
