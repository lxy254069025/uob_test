package ws

import (
	"encoding/json"
	"net"
	"net/http"
	"net/http/httptest"
	"net/url"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
)

const testSN = "SN-0001"

// pipeListener 是纯内存的 net.Listener：dial 建立一对 net.Pipe，
// 一端交给 Accept 返回，另一端给拨号方。测试因此不需要监听任何真实端口。
type pipeListener struct {
	conns  chan net.Conn
	closed chan struct{}
	once   sync.Once
}

func newPipeListener() *pipeListener {
	return &pipeListener{
		conns:  make(chan net.Conn),
		closed: make(chan struct{}),
	}
}

func (l *pipeListener) Accept() (net.Conn, error) {
	select {
	case c := <-l.conns:
		return c, nil
	case <-l.closed:
		return nil, net.ErrClosed
	}
}

func (l *pipeListener) Close() error {
	l.once.Do(func() { close(l.closed) })
	return nil
}

func (l *pipeListener) Addr() net.Addr { return pipeAddr{} }

// dial 建立一条内存连接并完成 websocket 客户端握手。
func (l *pipeListener) dial(query string, header http.Header) (*websocket.Conn, *http.Response, error) {
	client, server := net.Pipe()

	select {
	case l.conns <- server:
	case <-l.closed:
		_ = client.Close()
		_ = server.Close()
		return nil, nil, net.ErrClosed
	}

	u := &url.URL{
		Scheme:   "ws",
		Host:     "pipe",
		Path:     "/v1/ws/machine",
		RawQuery: query,
	}

	return websocket.NewClient(client, u, header, 1024, 1024)
}

type pipeAddr struct{}

func (pipeAddr) Network() string { return "pipe" }
func (pipeAddr) String() string  { return "pipe" }

type testEnv struct {
	hub *Hub
	r   *gin.Engine
	l   *pipeListener
}

func newTestEnv(t *testing.T, opts Options) *testEnv {
	t.Helper()

	hub := NewHub(opts)

	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.GET("/v1/ws/machine", hub.Handle)
	r.POST("/v1/ws/push", hub.HandlePush)

	l := newPipeListener()
	srv := &http.Server{Handler: r}
	go func() { _ = srv.Serve(l) }()

	t.Cleanup(func() {
		_ = srv.Close()
		_ = l.Close()
	})

	return &testEnv{hub: hub, r: r, l: l}
}

// do 用 gin 引擎直接处理请求，避免依赖真实端口。
func (e *testEnv) do(method, path, body string, header http.Header) *httptest.ResponseRecorder {
	req := httptest.NewRequest(method, path, strings.NewReader(body))
	req.Header.Set("Content-Type", "application/json")
	for k, vs := range header {
		req.Header[k] = vs
	}

	w := httptest.NewRecorder()
	e.r.ServeHTTP(w, req)

	return w
}

// waitFor 轮询等待条件成立，避免依赖固定 sleep。
func waitFor(t *testing.T, timeout time.Duration, cond func() bool) bool {
	t.Helper()

	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if cond() {
			return true
		}
		time.Sleep(10 * time.Millisecond)
	}

	return cond()
}

func TestHandleRejectsMissingSN(t *testing.T) {
	env := newTestEnv(t, Options{})

	if got := env.do(http.MethodGet, "/v1/ws/machine", "", nil).Code; got != http.StatusBadRequest {
		t.Fatalf("缺少 sn 期望 400，实际 %d", got)
	}
}

func TestHandleRejectsWrongToken(t *testing.T) {
	env := newTestEnv(t, Options{Token: "secret"})

	got := env.do(http.MethodGet, "/v1/ws/machine?sn="+testSN+"&token=wrong", "", nil).Code
	if got != http.StatusUnauthorized {
		t.Fatalf("token 不匹配期望 401，实际 %d", got)
	}
}

func TestHandleRejectsBrowserOriginNotInAllowList(t *testing.T) {
	env := newTestEnv(t, Options{AllowedOrigins: []string{"https://admin.example.com"}})

	got := env.do(http.MethodGet, "/v1/ws/machine?sn="+testSN, "", handshakeHeader("https://evil.example.com")).Code
	if got != http.StatusForbidden {
		t.Fatalf("未在白名单中的来源期望 403，实际 %d", got)
	}
}

// handshakeHeader 构造带 Origin 的完整 websocket 握手头，
// 让请求能走到 CheckOrigin 这一步（否则会先因缺少握手头返回 400）。
func handshakeHeader(origin string) http.Header {
	return http.Header{
		"Origin":                []string{origin},
		"Connection":            []string{"Upgrade"},
		"Upgrade":               []string{"websocket"},
		"Sec-Websocket-Version": []string{"13"},
		"Sec-Websocket-Key":     []string{"dGhlIHNhbXBsZSBub25jZQ=="},
	}
}

func TestHandleAcceptsNonBrowserClientWithoutOrigin(t *testing.T) {
	env := newTestEnv(t, Options{})

	conn, _, err := env.l.dial("sn="+testSN, nil)
	if err != nil {
		t.Fatalf("设备端（无 Origin）应该连接成功: %v", err)
	}
	defer conn.Close()

	if !waitFor(t, 2*time.Second, func() bool { return env.hub.IsOnline(testSN) }) {
		t.Fatal("连接建立后设备应处于在线状态")
	}
}

func TestHandleAcceptsAllowListedOrigin(t *testing.T) {
	env := newTestEnv(t, Options{AllowedOrigins: []string{"https://admin.example.com"}})

	header := http.Header{"Origin": []string{"https://admin.example.com"}}
	conn, _, err := env.l.dial("sn="+testSN, header)
	if err != nil {
		t.Fatalf("白名单内的来源应该连接成功: %v", err)
	}
	defer conn.Close()
}

func TestRegisterMessageRebindsSN(t *testing.T) {
	env := newTestEnv(t, Options{})

	received := make(chan Message, 4)
	env.hub.OnMessage(func(_ *Client, msg Message) { received <- msg })

	conn, _, err := env.l.dial("sn=temp-conn", nil)
	if err != nil {
		t.Fatalf("连接失败: %v", err)
	}
	defer conn.Close()

	// 设备先用临时标识建连，再上报真实序列号。
	if err := conn.WriteJSON(Message{Type: TypeRegister, SN: testSN}); err != nil {
		t.Fatalf("发送 register 失败: %v", err)
	}

	select {
	case msg := <-received:
		if msg.Type != TypeRegister || msg.SN != testSN {
			t.Fatalf("收到的报文不符合预期: %+v", msg)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("超时未收到上行报文")
	}

	if !waitFor(t, 2*time.Second, func() bool { return env.hub.IsOnline(testSN) }) {
		t.Fatal("上报 sn 后应按新序列号在线")
	}
	if env.hub.IsOnline("temp-conn") {
		t.Fatal("旧的临时序列号不应再被视为在线")
	}
}

func TestSendDownstreamAndPushEndpoint(t *testing.T) {
	env := newTestEnv(t, Options{})

	conn, _, err := env.l.dial("sn="+testSN, nil)
	if err != nil {
		t.Fatalf("连接失败: %v", err)
	}
	defer conn.Close()

	if !waitFor(t, 2*time.Second, func() bool { return env.hub.IsOnline(testSN) }) {
		t.Fatal("设备应处于在线状态")
	}

	// 直接调用 Hub 下发。
	if _, err := env.hub.Send(testSN, Message{Type: TypeCommand, Data: json.RawMessage(`{"action":"drop"}`)}); err != nil {
		t.Fatalf("Send 失败: %v", err)
	}

	_ = conn.SetReadDeadline(time.Now().Add(3 * time.Second))
	var got Message
	if err := conn.ReadJSON(&got); err != nil {
		t.Fatalf("读取下行报文失败: %v", err)
	}
	if got.Type != TypeCommand || string(got.Data) != `{"action":"drop"}` {
		t.Fatalf("下行报文不符合预期: %+v", got)
	}

	// 通过 HTTP 接口下发，未指定 type 时默认 command。
	w := env.do(http.MethodPost, "/v1/ws/push", `{"sn":"`+testSN+`","data":{"action":"reboot"}}`, nil)
	if w.Code != http.StatusOK {
		t.Fatalf("push 接口期望 200，实际 %d: %s", w.Code, w.Body.String())
	}

	_ = conn.SetReadDeadline(time.Now().Add(3 * time.Second))
	if err := conn.ReadJSON(&got); err != nil {
		t.Fatalf("读取 push 下行报文失败: %v", err)
	}
	if got.Type != TypeCommand || string(got.Data) != `{"action":"reboot"}` {
		t.Fatalf("push 下行报文不符合预期: %+v", got)
	}
}

func TestSendToOfflineMachine(t *testing.T) {
	env := newTestEnv(t, Options{})

	sent, err := env.hub.Send("not-connected", Message{Type: TypeCommand})
	if err != nil {
		t.Fatalf("Send 失败: %v", err)
	}
	if sent != 0 {
		t.Fatalf("离线设备不应有连接被投递，实际 %d", sent)
	}
}

func TestUnregisterOnDisconnect(t *testing.T) {
	env := newTestEnv(t, Options{})

	conn, _, err := env.l.dial("sn="+testSN, nil)
	if err != nil {
		t.Fatalf("连接失败: %v", err)
	}

	if !waitFor(t, 2*time.Second, func() bool { return env.hub.IsOnline(testSN) }) {
		t.Fatal("设备应处于在线状态")
	}

	_ = conn.Close()

	if !waitFor(t, 3*time.Second, func() bool { return !env.hub.IsOnline(testSN) }) {
		t.Fatal("断开后设备应下线")
	}
}

func TestPushEndpointValidatesBody(t *testing.T) {
	env := newTestEnv(t, Options{})

	if got := env.do(http.MethodPost, "/v1/ws/push", `{}`, nil).Code; got != http.StatusBadRequest {
		t.Fatalf("缺少 sn 期望 400，实际 %d", got)
	}
}

func TestOptionsWithDefaults(t *testing.T) {
	got := Options{}.withDefaults()
	want := DefaultOptions()

	if got.ReadLimit != want.ReadLimit ||
		got.PingInterval != want.PingInterval ||
		got.PongTimeout != want.PongTimeout ||
		got.WriteTimeout != want.WriteTimeout ||
		got.SendBuffer != want.SendBuffer {
		t.Fatalf("零值参数应补成默认值，期望 %+v，实际 %+v", want, got)
	}

	custom := Options{PingInterval: time.Second, SendBuffer: 8}.withDefaults()
	if custom.PingInterval != time.Second || custom.SendBuffer != 8 {
		t.Fatalf("自定义参数不应被覆盖: %+v", custom)
	}
	if custom.ReadLimit != want.ReadLimit || custom.PongTimeout != want.PongTimeout {
		t.Fatalf("未设置的参数应补默认值: %+v", custom)
	}
}

func TestMessageEncodeSetsTimestamp(t *testing.T) {
	payload, err := Message{Type: TypeCommand}.Encode()
	if err != nil {
		t.Fatalf("Encode 失败: %v", err)
	}

	var got Message
	if err := json.Unmarshal(payload, &got); err != nil {
		t.Fatalf("解码失败: %v", err)
	}
	if got.TS == 0 {
		t.Fatal("Encode 应自动补上毫秒时间戳")
	}
	if got.Type != TypeCommand {
		t.Fatalf("type 应被保留，实际 %s", got.Type)
	}
}
