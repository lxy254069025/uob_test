package v1

import (
	"encoding/json"
	"testing"

	"uopenbox/martbox-api/ws"
)

// 用户给的真实报文样例：锁/摄像头/POS 三个在线模块，temperature 模块离线。
const statusBody = `{"battery":80,"volume":5,"networkType":"wifi","temperature":"","hardwareAvailable":true,"devices":[` +
	`{"module":"lock","online":true,"busy":false,"doorOpen":false,"lockOpen":false},` +
	`{"module":"camera","online":true,"busy":false},` +
	`{"module":"pos","online":true,"busy":false},` +
	`{"module":"temperature","online":false,"busy":false}]}`

func statusMessage(t *testing.T, sn, body string) ws.Message {
	t.Helper()

	if !json.Valid([]byte(body)) {
		t.Fatalf("测试数据不是合法 JSON: %s", body)
	}

	return ws.Message{Type: ws.TypeStatus, SN: sn, Data: json.RawMessage(body), TS: 1728000000000}
}

func TestParseMachineStatus(t *testing.T) {
	status, err := ParseMachineStatus([]byte(statusBody))
	if err != nil {
		t.Fatalf("解析失败: %v", err)
	}

	if status.Battery != 80 || status.Volume != 5 || !status.HardwareAvailable {
		t.Fatalf("整体状态解析不对: %+v", status)
	}
	if status.NetworkType != "wifi" {
		t.Fatalf("networkType 期望 wifi，实际 %q", status.NetworkType)
	}
	if status.Temperature.String() != "" {
		t.Fatalf("样例里的 temperature 是空串，实际 %q", status.Temperature.String())
	}
	if len(status.Devices) != 4 {
		t.Fatalf("模块数期望 4，实际 %d", len(status.Devices))
	}

	lock := status.Devices[0]
	if lock.Module != "lock" || !lock.Online || lock.Busy {
		t.Fatalf("lock 模块解析不对: %+v", lock)
	}
	// 锁模块才有这两个字段，要能区分"上报了 false"和"没上报"。
	if lock.DoorOpen == nil || *lock.DoorOpen || lock.LockOpen == nil || *lock.LockOpen {
		t.Fatalf("lock 的 doorOpen/lockOpen 应为明确的 false: %+v", lock)
	}

	camera := status.Devices[1]
	if camera.Module != "camera" || camera.DoorOpen != nil || camera.LockOpen != nil {
		t.Fatalf("camera 不该有锁相关字段: %+v", camera)
	}

	if got := status.Devices[3]; got.Module != "temperature" || got.Online {
		t.Fatalf("temperature 模块应解析为离线: %+v", got)
	}
}

func TestOfflineModules(t *testing.T) {
	sample, err := ParseMachineStatus([]byte(statusBody))
	if err != nil {
		t.Fatalf("解析失败: %v", err)
	}
	if got := OfflineModules(sample); len(got) != 1 || got[0] != "temperature" {
		t.Fatalf("样例里只有 temperature 离线，实际 %v", got)
	}

	allOnline, err := ParseMachineStatus([]byte(`{"devices":[` +
		`{"module":"lock","online":true,"busy":false},` +
		`{"module":"camera","online":true,"busy":false}]}`))
	if err != nil {
		t.Fatalf("解析失败: %v", err)
	}
	if got := OfflineModules(allOnline); len(got) != 0 {
		t.Fatalf("全部在线时不该有离线模块，实际 %v", got)
	}

	body := `{"battery":10,"volume":0,"hardwareAvailable":false,"devices":[` +
		`{"module":"lock","online":true,"busy":false},` +
		`{"module":"camera","online":false,"busy":false},` +
		`{"module":"pos","online":false,"busy":true}]}`
	withOffline, err := ParseMachineStatus([]byte(body))
	if err != nil {
		t.Fatalf("解析失败: %v", err)
	}

	got := OfflineModules(withOffline)
	if len(got) != 2 || got[0] != "camera" || got[1] != "pos" {
		t.Fatalf("离线模块识别错误: %v", got)
	}
}

// 温度设备可能发字符串也可能直接发数字，两种都不能让整份状态解析失败。
func TestTemperatureAcceptsStringAndNumber(t *testing.T) {
	cases := map[string]string{
		"空串":   `""`,
		"字符串":  `"26.5"`,
		"数字":   `26.5`,
		"整数":   `26`,
		"null": `null`,
	}

	for name, raw := range cases {
		t.Run(name, func(t *testing.T) {
			status, err := ParseMachineStatus([]byte(`{"battery":80,"temperature":` + raw + `}`))
			if err != nil {
				t.Fatalf("temperature=%s 时解析不该失败: %v", raw, err)
			}
			if status.Battery != 80 {
				t.Fatalf("其它字段应照常解析: %+v", status)
			}
		})
	}

	status, err := ParseMachineStatus([]byte(`{"temperature":26.5}`))
	if err != nil {
		t.Fatalf("解析失败: %v", err)
	}
	if status.Temperature.String() != "26.5" {
		t.Fatalf("数字应保留原文，实际 %q", status.Temperature.String())
	}
}

// status 报文是最新状态，也兼作应用层心跳，收到就该存下来。
func TestStatusMessageSavesSnapshot(t *testing.T) {
	const sn = "martbox-dev-0001"

	dispatchMachineMessage(sn, "127.0.0.1:1234", statusMessage(t, sn, statusBody))

	snapshot, ok := LatestMachineStatus(sn)
	if !ok {
		t.Fatal("收到 status 报文后应能查到最新状态")
	}
	if snapshot.Status.Battery != 80 || len(snapshot.Status.Devices) != 4 {
		t.Fatalf("保存的状态不对: %+v", snapshot.Status)
	}
	if snapshot.Status.NetworkType != "wifi" {
		t.Fatalf("新增的 networkType 应一起保存，实际 %q", snapshot.Status.NetworkType)
	}
	if snapshot.DeviceTS != 1728000000000 {
		t.Fatalf("设备时间戳应保留，实际 %d", snapshot.DeviceTS)
	}
	if snapshot.UpdatedAt.IsZero() {
		t.Fatal("应记录服务端收到时间")
	}
}

// 其它类型的上行报文不该被当成状态存下来。
func TestNonStatusMessageIsNotStored(t *testing.T) {
	const sn = "martbox-dev-0002"

	dispatchMachineMessage(sn, "127.0.0.1:1234", ws.Message{
		Type: ws.TypeEvent,
		SN:   sn,
		Data: json.RawMessage(`{"action":"drop"}`),
	})

	if _, ok := LatestMachineStatus(sn); ok {
		t.Fatal("event 报文不该写入状态快照")
	}
}

// 状态体坏掉时记日志就好，不能影响连接存活，也不能 panic。
func TestBrokenStatusBodyIsIgnored(t *testing.T) {
	const sn = "martbox-dev-0003"

	dispatchMachineMessage(sn, "127.0.0.1:1234", ws.Message{
		Type: ws.TypeStatus,
		SN:   sn,
		Data: json.RawMessage(`{"battery":`),
	})

	if _, ok := LatestMachineStatus(sn); ok {
		t.Fatal("非法状态体不该写入快照")
	}
}
