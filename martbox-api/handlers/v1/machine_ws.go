package v1

import (
	"encoding/json"
	"log"
	"strings"
	"sync"
	"time"

	"uopenbox/martbox-api/ws"
)

// MachineStatus 是设备定时上报的状态，同时充当应用层心跳：
// 设备每隔一段时间发一条，服务端收到就说明它还活着（超时判定见 ws.HeartbeatTimeout）。
type MachineStatus struct {
	Battery           int            `json:"battery"`           // 电量百分比
	Volume            int            `json:"volume"`            // 音量
	NetworkType       string         `json:"networkType"`       // 网络类型，例如 wifi / 4g
	Temperature       StringOrNumber `json:"temperature"`       // 温度，没采集到就是空字符串
	HardwareAvailable bool           `json:"hardwareAvailable"` // 硬件整体是否可用
	Devices           []ModuleStatus `json:"devices"`           // 各模块状态
}

// StringOrNumber 兼容"字符串或数字"两种上报形式。
//
// temperature 的样例给的是 ""，但温度这类字段设备也可能直接发 26.5；
// 一旦类型对不上，json.Unmarshal 会让整份状态解析失败，状态和快照都丢掉，
// 所以这里放宽：数字按原文保留，取用时再转换。
type StringOrNumber string

func (s *StringOrNumber) UnmarshalJSON(data []byte) error {
	trimmed := strings.TrimSpace(string(data))
	if trimmed == "" || trimmed == "null" {
		*s = ""
		return nil
	}

	if data[0] == '"' {
		var str string
		if err := json.Unmarshal(data, &str); err != nil {
			return err
		}
		*s = StringOrNumber(str)
		return nil
	}

	// 数字等非字符串值原样保留文本形式。
	*s = StringOrNumber(trimmed)

	return nil
}

// String 返回上报的原文，空字符串表示设备没提供该值。
func (s StringOrNumber) String() string {
	return string(s)
}

// ModuleStatus 是单个模块（锁、摄像头、POS 等）的状态。
// doorOpen / lockOpen 只有锁模块才有，用指针区分"没上报"和"明确为 false"。
type ModuleStatus struct {
	Module   string `json:"module"`
	Online   bool   `json:"online"`
	Busy     bool   `json:"busy"`
	DoorOpen *bool  `json:"doorOpen,omitempty"`
	LockOpen *bool  `json:"lockOpen,omitempty"`
}

// Offline 表示该模块当前不可用。
func (m ModuleStatus) Offline() bool {
	return !m.Online
}

// OnMachineMessage 按报文类型分发设备上行报文。
//
// 目前处理 status（状态上报 / 应用层心跳），其余类型先只记日志，
// 后续接入 machines-rpc 时在这里按 msg.Type 逐个补上。
func OnMachineMessage(c *ws.Client, msg ws.Message) {
	dispatchMachineMessage(c.SN(), c.RemoteAddr(), msg)
}

// dispatchMachineMessage 与连接对象解耦，方便单测覆盖分发逻辑。
func dispatchMachineMessage(sn, addr string, msg ws.Message) {
	switch msg.Type {
	case ws.TypeStatus:
		handleMachineStatus(sn, addr, msg)
	default:
		log.Printf("ws: 收到设备报文 sn=%s type=%s addr=%s data=%s",
			sn, msg.Type, addr, string(msg.Data))
	}
}

// handleMachineStatus 处理设备状态上报。
//
// 心跳本身由 ws 的读循环统一刷新（收到任何数据都算活着），
// 这里只负责把状态留下、并把异常模块记进日志。
func handleMachineStatus(sn, addr string, msg ws.Message) {
	status, err := ParseMachineStatus(msg.Data)
	if err != nil {
		// 状态报文体坏了不影响连接存活（这一帧已经刷新过心跳），记日志即可。
		log.Printf("ws: 状态报文体不是合法 JSON sn=%s addr=%s err=%v body=%s",
			sn, addr, err, string(msg.Data))
		return
	}

	saveMachineStatus(sn, status, msg.TS)

	if offline := OfflineModules(status); len(offline) > 0 {
		log.Printf("ws: 设备状态异常 sn=%s 电量=%d%% 音量=%d 网络=%s 温度=%q 硬件可用=%t 离线模块=%s",
			sn, status.Battery, status.Volume, status.NetworkType, status.Temperature.String(),
			status.HardwareAvailable, strings.Join(offline, ","))
		return
	}

	log.Printf("ws: 设备状态 sn=%s 电量=%d%% 音量=%d 网络=%s 温度=%q 硬件可用=%t 模块数=%d",
		sn, status.Battery, status.Volume, status.NetworkType, status.Temperature.String(),
		status.HardwareAvailable, len(status.Devices))
}

// ParseMachineStatus 解析状态报文的 data 部分。
func ParseMachineStatus(data []byte) (MachineStatus, error) {
	var status MachineStatus
	if err := json.Unmarshal(data, &status); err != nil {
		return MachineStatus{}, err
	}

	return status, nil
}

// OfflineModules 返回离线模块的名字，便于告警和日志。
func OfflineModules(status MachineStatus) []string {
	offline := make([]string, 0, len(status.Devices))
	for _, d := range status.Devices {
		if d.Offline() {
			offline = append(offline, d.Module)
		}
	}

	return offline
}

// MachineStatusSnapshot 是最近一次状态上报的快照。
type MachineStatusSnapshot struct {
	Status    MachineStatus
	DeviceTS  int64     // 设备侧时间戳，毫秒
	UpdatedAt time.Time // 服务端收到的时间
}

// 最近一次状态只在内存里保留，进程重启即丢。
// TODO: 接入 machines-rpc / 存储后改成落库，这里只作为过渡。
var (
	machineStatusMu sync.RWMutex
	machineStatuses = make(map[string]MachineStatusSnapshot)
)

func saveMachineStatus(sn string, status MachineStatus, deviceTS int64) {
	if sn == "" {
		return
	}

	machineStatusMu.Lock()
	machineStatuses[sn] = MachineStatusSnapshot{
		Status:    status,
		DeviceTS:  deviceTS,
		UpdatedAt: time.Now(),
	}
	machineStatusMu.Unlock()
}

// LatestMachineStatus 返回某台设备最近一次上报的状态。
func LatestMachineStatus(sn string) (MachineStatusSnapshot, bool) {
	machineStatusMu.RLock()
	defer machineStatusMu.RUnlock()

	snapshot, ok := machineStatuses[sn]

	return snapshot, ok
}

// OnMachineOffline 处理设备断线，reason 说明原因（心跳超时、客户端主动关闭等）。
func OnMachineOffline(c *ws.Client, reason string) {
	log.Printf("ws: 设备离线 sn=%s addr=%s 原因=%s", c.SN(), c.RemoteAddr(), reason)
}
