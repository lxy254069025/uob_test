package ws

import (
	"errors"
	"log"
	"net/http"
	"strings"

	"github.com/gin-gonic/gin"
)

// Handle 是 Gin 的升级入口。启用 Authenticate 时，握手请求用
// Authorization: Bearer <token> 鉴权，并从 token 解析设备 SN；
// 未启用时保留旧的 ?sn / ?token 查询参数模式。
// 升级成功后连接交给 Hub 管理，函数返回时连接仍然存活。
func (h *Hub) Handle(c *gin.Context) {
	sn, err := h.authenticate(c.Request)
	if err != nil {
		log.Printf("ws: 设备鉴权失败 addr=%s err=%v", c.Request.RemoteAddr, err)
		c.JSON(http.StatusUnauthorized, gin.H{"error": "token 校验失败"})
		return
	}

	sn = strings.TrimSpace(sn)
	if sn == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "缺少设备序列号 sn"})
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

// authenticate 返回握手请求对应的设备 SN。
// 优先使用外部注入的鉴权函数；否则退回旧的查询参数鉴权。
func (h *Hub) authenticate(r *http.Request) (string, error) {
	if h.opts.Authenticate != nil {
		return h.opts.Authenticate(r)
	}

	if h.opts.Token != "" && r.URL.Query().Get("token") != h.opts.Token {
		return "", errors.New("token 校验失败")
	}

	return r.URL.Query().Get("sn"), nil
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
