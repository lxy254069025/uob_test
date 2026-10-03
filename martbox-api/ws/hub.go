package ws

import (
	"log"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

// Options 是 Hub 的运行参数，零值会被 withDefaults 补成默认值。
type Options struct {
	Token          string        // 非空时，连接必须带 ?token= 且一致
	AllowedOrigins []string      // 允许的浏览器 Origin，"*" 表示不限制；为空表示只允许非浏览器客户端
	ReadLimit      int64         // 单条上行报文大小上限，字节
	PingInterval   time.Duration // 服务端心跳间隔
	PongTimeout    time.Duration // 心跳响应超时，超时即断开
	WriteTimeout   time.Duration // 单次写超时
	SendBuffer     int           // 单连接发送队列长度
}

// DefaultOptions 返回一套适合设备长连接的默认参数。
func DefaultOptions() Options {
	return Options{
		ReadLimit:    16 << 10, // 16KB，设备上行报文一般很小
		PingInterval: 30 * time.Second,
		PongTimeout:  90 * time.Second, // 至少 2 倍心跳间隔，容忍一次丢包
		WriteTimeout: 10 * time.Second,
		SendBuffer:   64,
	}
}

func (o Options) withDefaults() Options {
	d := DefaultOptions()

	if o.ReadLimit <= 0 {
		o.ReadLimit = d.ReadLimit
	}
	if o.PingInterval <= 0 {
		o.PingInterval = d.PingInterval
	}
	if o.PongTimeout <= 0 {
		o.PongTimeout = d.PongTimeout
	}
	if o.WriteTimeout <= 0 {
		o.WriteTimeout = d.WriteTimeout
	}
	if o.SendBuffer <= 0 {
		o.SendBuffer = d.SendBuffer
	}

	return o
}

// MessageHandler 处理设备上行报文，由业务方注册。
type MessageHandler func(c *Client, msg Message)

var (
	globalMu  sync.RWMutex
	globalHub = NewHub(DefaultOptions())
)

// Default 返回全局 Hub，业务代码通过它给设备下发指令或查询在线状态。
func Default() *Hub {
	globalMu.RLock()
	defer globalMu.RUnlock()

	return globalHub
}

// Init 用配置创建全局 Hub，由 router 在启动时调用一次。
func Init(opts Options) *Hub {
	globalMu.Lock()
	defer globalMu.Unlock()

	globalHub = NewHub(opts)

	return globalHub
}

// Hub 维护所有设备长连接，key 为设备序列号 sn。
type Hub struct {
	mu      sync.RWMutex
	clients map[string]map[*Client]struct{}
	opts    Options

	upgrader websocket.Upgrader

	handlerMu sync.RWMutex
	handler   MessageHandler
}

func NewHub(opts Options) *Hub {
	h := &Hub{
		clients: make(map[string]map[*Client]struct{}),
		opts:    opts.withDefaults(),
	}
	h.upgrader = websocket.Upgrader{
		HandshakeTimeout: 10 * time.Second,
		ReadBufferSize:   1024,
		WriteBufferSize:  1024,
		CheckOrigin:      h.checkOrigin,
	}

	return h
}

// OnMessage 注册上行报文处理函数，建议在设备连接之前调用。
func (h *Hub) OnMessage(handler MessageHandler) {
	h.handlerMu.Lock()
	h.handler = handler
	h.handlerMu.Unlock()
}

func (h *Hub) handleMessage(c *Client, msg Message) {
	h.handlerMu.RLock()
	handler := h.handler
	h.handlerMu.RUnlock()

	if handler == nil {
		log.Printf("ws: sn=%s 收到 %s 报文，但未注册处理函数，已忽略", c.SN(), msg.Type)
		return
	}

	handler(c, msg)
}

// Options 返回当前生效的运行参数。
func (h *Hub) Options() Options {
	return h.opts
}

func (h *Hub) register(c *Client) {
	h.mu.Lock()
	defer h.mu.Unlock()

	// 锁内读取 sn：rebind/unregister 同样持锁，保证不会和重绑定交错。
	sn := c.SN()
	if sn == "" {
		return
	}

	if h.clients[sn] == nil {
		h.clients[sn] = make(map[*Client]struct{})
	}
	h.clients[sn][c] = struct{}{}
}

func (h *Hub) unregister(c *Client) {
	h.mu.Lock()
	defer h.mu.Unlock()

	sn := c.SN()
	if sn == "" {
		return
	}

	conns, ok := h.clients[sn]
	if !ok {
		return
	}

	delete(conns, c)
	if len(conns) == 0 {
		delete(h.clients, sn)
	}
}

// rebind 在设备上报 sn 后把连接从旧的 key 挪到新的 key。
func (h *Hub) rebind(c *Client, sn string) {
	h.mu.Lock()
	defer h.mu.Unlock()

	old := c.SN()
	if old != "" {
		if conns, ok := h.clients[old]; ok {
			delete(conns, c)
			if len(conns) == 0 {
				delete(h.clients, old)
			}
		}
	}
	if h.clients[sn] == nil {
		h.clients[sn] = make(map[*Client]struct{})
	}
	h.clients[sn][c] = struct{}{}
	c.setSN(sn)
}

// Send 把报文下发给指定设备的所有连接，返回成功入队的连接数。
func (h *Hub) Send(sn string, msg Message) (int, error) {
	payload, err := msg.Encode()
	if err != nil {
		return 0, err
	}

	h.mu.RLock()
	conns := make([]*Client, 0, len(h.clients[sn]))
	for c := range h.clients[sn] {
		conns = append(conns, c)
	}
	h.mu.RUnlock()

	sent := 0
	for _, c := range conns {
		if c.enqueue(payload) {
			sent++
		}
	}

	return sent, nil
}

// IsOnline 判断设备当前是否有活跃连接。
func (h *Hub) IsOnline(sn string) bool {
	h.mu.RLock()
	defer h.mu.RUnlock()

	return len(h.clients[sn]) > 0
}

// Online 返回当前在线的设备序列号列表，便于监控和排查。
func (h *Hub) Online() []string {
	h.mu.RLock()
	defer h.mu.RUnlock()

	sns := make([]string, 0, len(h.clients))
	for sn := range h.clients {
		sns = append(sns, sn)
	}

	return sns
}
