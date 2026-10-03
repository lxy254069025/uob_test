package ws

import (
	"encoding/json"
	"log"
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

// readPump 负责读设备上行报文。退出时注销连接，保证 Hub 里不残留死连接。
func (c *Client) readPump() {
	opts := c.hub.opts

	defer func() {
		c.hub.unregister(c)
		c.close()
	}()

	c.conn.SetReadLimit(opts.ReadLimit)
	_ = c.conn.SetReadDeadline(time.Now().Add(opts.PongTimeout))
	c.conn.SetPongHandler(func(string) error {
		return c.conn.SetReadDeadline(time.Now().Add(opts.PongTimeout))
	})

	for {
		_, payload, err := c.conn.ReadMessage()
		if err != nil {
			if websocket.IsUnexpectedCloseError(err, websocket.CloseNormalClosure, websocket.CloseGoingAway) {
				log.Printf("ws: sn=%s addr=%s 连接异常关闭: %v", c.SN(), c.remoteAddr, err)
			}
			return
		}

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

// writePump 负责下发数据和心跳。所有写操作都集中在这里，避免并发写同一个连接。
func (c *Client) writePump() {
	opts := c.hub.opts
	ticker := time.NewTicker(opts.PingInterval)

	defer func() {
		ticker.Stop()
		c.close()
	}()

	for {
		select {
		case payload, ok := <-c.send:
			_ = c.conn.SetWriteDeadline(time.Now().Add(opts.WriteTimeout))
			if !ok {
				_ = c.conn.WriteMessage(websocket.CloseMessage, []byte{})
				return
			}

			if err := c.conn.WriteMessage(websocket.TextMessage, payload); err != nil {
				return
			}
		case <-ticker.C:
			_ = c.conn.SetWriteDeadline(time.Now().Add(opts.WriteTimeout))
			if err := c.conn.WriteMessage(websocket.PingMessage, nil); err != nil {
				return
			}
		}
	}
}
