package ws

import (
	"encoding/json"
	"time"
)

// 报文类型。上行（设备 -> 服务端）和下行（服务端 -> 设备）共用一套 type，
// 设备端按 type 分发即可，新增类型不会影响旧客户端。
const (
	TypeRegister = "register" // 设备上行：注册，data 里可以带设备信息
	TypeEvent    = "event"    // 设备上行：事件上报（出货、故障、投币等）
	TypeAck      = "ack"      // 双向：某条消息的回执，用 id 关联原消息
	TypeCommand  = "command"  // 服务端下行：指令（出货、重启、改配置等）
	TypeError    = "error"    // 服务端下行：错误提示
)

// Message 是设备与服务端之间统一的 JSON 报文。
type Message struct {
	Type string          `json:"type"`
	SN   string          `json:"sn,omitempty"`   // 设备序列号
	ID   string          `json:"id,omitempty"`   // 消息 ID，ack 时原样带回
	Data json.RawMessage `json:"data,omitempty"` // 业务数据，结构由 type 决定
	TS   int64           `json:"ts,omitempty"`   // 毫秒时间戳
}

// NewMessage 构造一条下行报文，data 会被序列化成 json.RawMessage。
func NewMessage(typ, sn, id string, data interface{}) (Message, error) {
	msg := Message{
		Type: typ,
		SN:   sn,
		ID:   id,
		TS:   time.Now().UnixMilli(),
	}

	if data != nil {
		raw, err := json.Marshal(data)
		if err != nil {
			return Message{}, err
		}
		msg.Data = raw
	}

	return msg, nil
}

// Encode 序列化为可直接发送的字节。
func (m Message) Encode() ([]byte, error) {
	if m.TS == 0 {
		m.TS = time.Now().UnixMilli()
	}

	return json.Marshal(m)
}
