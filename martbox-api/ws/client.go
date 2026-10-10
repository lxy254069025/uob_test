package ws

import (
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"net"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

// Client 代表一条设备长连接。同一个设备（sn）可能同时存在多条连接，
// 例如设备重连时旧连接还没超时，因此 Hub 里用集合保存。
type Client struct {
	hub        *Hub
	conn       *websocket.Conn
	send       chan []byte
	remoteAddr string

	mu     sync.RWMutex
	sn     string
	closed bool
}

func newClient(hub *Hub, conn *websocket.Conn, sn string) *Client {
	return &Client{
		hub:        hub,
		conn:       conn,
		send:       make(chan []byte, hub.opts.SendBuffer),
		remoteAddr: conn.RemoteAddr().String(),
		sn:         sn,
	}
}

// SN 返回连接当前绑定的设备序列号。
func (c *Client) SN() string {
	c.mu.RLock()
	defer c.mu.RUnlock()

	return c.sn
}

// RemoteAddr 返回对端地址，便于日志排查。
func (c *Client) RemoteAddr() string {
	return c.remoteAddr
}

func (c *Client) setSN(sn string) {
	c.mu.Lock()
	c.sn = sn
	c.mu.Unlock()
}

// close 关闭底层连接，可重复调用。
func (c *Client) close() {
	c.mu.Lock()
	if c.closed {
		c.mu.Unlock()
		return
	}
	c.closed = true
	c.mu.Unlock()

	_ = c.conn.Close()
}

// enqueue 把待发送数据放入队列；队列满说明设备消费不过来，直接断开，避免拖垮服务端。
func (c *Client) enqueue(payload []byte) bool {
	select {
	case c.send <- payload:
		return true
	default:
		log.Printf("ws: sn=%s addr=%s 发送队列已满，断开连接", c.SN(), c.remoteAddr)
		c.hub.unregister(c)
		c.close()
		return false
	}
}

// readPump 负责读设备上行报文和心跳。
//
// 心跳方向：客户端定时发 ping，服务端收到后按 RFC 6455 回 pong；
// 服务端自己不主动 ping。超过 HeartbeatTimeout 没收到客户端任何数据（含心跳）
// 就判定掉线，断开连接并触发断线处理。
func (c *Client) readPump() {
	opts := c.hub.opts
	reason := reasonClientClosed

	defer func() {
		c.hub.unregister(c)
		c.hub.notifyDisconnect(c, reason)
		c.close()
	}()

	c.conn.SetReadLimit(opts.ReadLimit)
	c.touch()

	// 客户端的心跳：回 pong，并借这次心跳刷新读超时。
	c.conn.SetPingHandler(func(appData string) error {
		c.touch()
		return c.conn.WriteControl(websocket.PongMessage, []byte(appData), time.Now().Add(opts.WriteTimeout))
	})
	// 兼容仍然发 pong 的客户端：pong 同样算活着。
	c.conn.SetPongHandler(func(string) error {
		c.touch()
		return nil
	})

	for {
		_, payload, err := c.conn.ReadMessage()
		if err != nil {
			reason = classifyDisconnect(err, opts.HeartbeatTimeout)
			return
		}

		// 有业务数据上来同样说明设备还活着。
		c.touch()

		var msg Message
		if err := json.Unmarshal(payload, &msg); err != nil {
			log.Printf("ws: sn=%s addr=%s 报文不是合法 JSON: %v", c.SN(), c.remoteAddr, err)
			continue
		}

		// 设备可以在报文里带上 sn，用于重连后重新绑定（例如先建连再上报序列号）。
		if msg.SN != "" && msg.SN != c.SN() {
			c.hub.rebind(c, msg.SN)
		}

		c.hub.handleMessage(c, msg)
	}
}

const reasonClientClosed = "客户端主动关闭"

// touch 刷新读超时，表示刚刚收到过数据或心跳。
func (c *Client) touch() {
	_ = c.conn.SetReadDeadline(time.Now().Add(c.hub.opts.HeartbeatTimeout))
}

// classifyDisconnect 把读错误翻译成断线原因，便于日志和业务侧判断。
func classifyDisconnect(err error, heartbeatTimeout time.Duration) string {
	var netErr net.Error
	if errors.As(err, &netErr) && netErr.Timeout() {
		return fmt.Sprintf("心跳超时（%v 内未收到客户端心跳）", heartbeatTimeout)
	}
	if websocket.IsCloseError(err, websocket.CloseNormalClosure, websocket.CloseGoingAway) {
		return reasonClientClosed
	}

	return err.Error()
}

// writePump 负责下发数据和心跳。所有写操作都集中在这里，避免并发写同一个连接。
func (c *Client) writePump() {
	opts := c.hub.opts

	defer c.close()

	for {
		payload, ok := <-c.send
		_ = c.conn.SetWriteDeadline(time.Now().Add(opts.WriteTimeout))
		if !ok {
			_ = c.conn.WriteMessage(websocket.CloseMessage, []byte{})
			return
		}

		if err := c.conn.WriteMessage(websocket.TextMessage, payload); err != nil {
			return
		}
	}
}
