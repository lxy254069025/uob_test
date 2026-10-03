package v1

import (
	"log"

	"uopenbox/martbox-api/ws"
)

// OnMachineMessage 处理设备上行报文。
//
// 目前只做日志，后续接入 machines-rpc 时在这里按 msg.Type 分发：
//   - ws.TypeRegister：记录设备上线信息（ip、version 等）
//   - ws.TypeEvent：出货结果、故障、投币等业务事件
//   - ws.TypeAck：服务端指令的回执，按 msg.ID 关联
func OnMachineMessage(c *ws.Client, msg ws.Message) {
	log.Printf("ws: 收到设备报文 sn=%s type=%s addr=%s data=%s",
		c.SN(), msg.Type, c.RemoteAddr(), string(msg.Data))
}
