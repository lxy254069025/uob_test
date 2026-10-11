package unigate

import (
	"bytes"
	"context"
	"errors"
	"io"
	"log"
	"net/http"
	"strconv"
	"strings"
	"testing"
)

// stubTransport 让测试不用真的监听端口，同时能拿到实际发出的请求体。
type stubTransport struct {
	status   int
	body     string
	lastBody []byte
}

func (t *stubTransport) RoundTrip(req *http.Request) (*http.Response, error) {
	if req.Body != nil {
		b, err := io.ReadAll(req.Body)
		if err != nil {
			return nil, err
		}
		t.lastBody = b
	}

	return &http.Response{
		StatusCode: t.status,
		Header:     make(http.Header),
		Body:       io.NopCloser(strings.NewReader(t.body)),
		Request:    req,
	}, nil
}

func newStubClient(t *testing.T, status int, body string) (*Client, *stubTransport) {
	t.Helper()

	transport := &stubTransport{status: status, body: body}
	client := NewClient("https://example.invalid/Unigate", "CODE", "USER", "PASS")
	client.HTTPClient = &http.Client{Transport: transport}

	return client, transport
}

func captureLog(t *testing.T, fn func()) string {
	t.Helper()

	var buf bytes.Buffer
	prev := log.Writer()
	log.SetOutput(&buf)
	defer log.SetOutput(prev)

	fn()

	return buf.String()
}

// 线上真实出现过的那次报错：500 + UnknownError + 空 message。
func TestAPIErrorSurfacesCodeAndTraceID(t *testing.T) {
	body := `{"magTranID":"","transactionUTCTimeStamp":null,` +
		`"customerTransactionID":"d72a731c4cb0e2fbda0f82580ac1c8f9","additionalFaultData":null,` +
		`"traceID":"ad525361-a643-4fbc-981a-5117b3279b19","code":"UnknownError","message":""}`
	client, _ := newStubClient(t, http.StatusInternalServerError, body)

	_, err := client.EMVAuthorize(context.Background(), "order-1", 100, "arqc")
	if err == nil {
		t.Fatal("500 应返回错误")
	}

	var apiErr *APIError
	if !errors.As(err, &apiErr) {
		t.Fatalf("错误类型应为 *APIError，实际 %T", err)
	}
	if apiErr.Code != "UnknownError" {
		t.Fatalf("code 没解析出来: %+v", apiErr)
	}
	if apiErr.TraceID != "ad525361-a643-4fbc-981a-5117b3279b19" {
		t.Fatalf("traceID 没解析出来: %+v", apiErr)
	}

	msg := err.Error()
	if !strings.Contains(msg, "UnknownError") || !strings.Contains(msg, apiErr.TraceID) {
		t.Fatalf("错误信息应包含错误码和 traceID，实际 %s", msg)
	}
	if !strings.Contains(msg, "POST /api/Transaction/EMV") {
		t.Fatalf("错误信息应包含出错的接口路径，实际 %s", msg)
	}
	if strings.Contains(msg, "customerTransactionID") {
		t.Fatalf("错误信息不该再拼整坨 JSON，实际 %s", msg)
	}
}

func TestAPIErrorParsesValidationFault(t *testing.T) {
	body := `{"traceID":"c1d35cdb-0a36-4571-9d7c-83464fd468fe","code":"InvalidPaymentMode",` +
		`"message":"PaymentMode is not supported - Invalid value [] for tag DFDF52 in ARQC"}`
	client, _ := newStubClient(t, http.StatusBadRequest, body)

	_, err := client.EMVAuthorize(context.Background(), "order-1", 100, "arqc")

	var apiErr *APIError
	if !errors.As(err, &apiErr) {
		t.Fatalf("错误类型应为 *APIError，实际 %T", err)
	}
	if apiErr.Code != "InvalidPaymentMode" || !strings.Contains(apiErr.Message, "DFDF52") {
		t.Fatalf("fault 字段没解析出来: %+v", apiErr)
	}
	if !strings.Contains(err.Error(), "DFDF52") {
		t.Fatalf("错误信息应带上 Magensa 的描述，实际 %s", err)
	}
}

// 网关返回 HTML 之类的非 JSON 响应时，保留原文，别把现场丢掉。
func TestAPIErrorFallsBackToRawBody(t *testing.T) {
	client, _ := newStubClient(t, http.StatusBadGateway, "<html>502 Bad Gateway</html>")

	_, err := client.EMVAuthorize(context.Background(), "order-1", 100, "arqc")

	var apiErr *APIError
	if !errors.As(err, &apiErr) {
		t.Fatalf("错误类型应为 *APIError，实际 %T", err)
	}
	if apiErr.Code != "" || apiErr.TraceID != "" {
		t.Fatalf("非 JSON 响应不该解析出字段: %+v", apiErr)
	}
	if !strings.Contains(err.Error(), "502 Bad Gateway") {
		t.Fatalf("解析不了结构时应保留原始响应体，实际 %s", err)
	}
}

func TestDoJSONDecodesSuccess(t *testing.T) {
	body := `{"magTranID":"MAG123","transactionOutput":{"isTransactionApproved":true,"transactionStatus":"Approved"}}`
	client, _ := newStubClient(t, http.StatusOK, body)

	resp, err := client.EMVAuthorize(context.Background(), "order-1", 100, "arqc")
	if err != nil {
		t.Fatalf("成功响应不该报错: %v", err)
	}
	if resp.MagTranID != "MAG123" || !resp.TransactionOutput.IsTransactionApproved {
		t.Fatalf("响应没解析正确: %+v", resp)
	}
}

// 打开请求体日志时 ARQC 必须脱敏，但真正发出去的报文不能被改动。
func TestRequestBodyLoggingRedactsARQC(t *testing.T) {
	// 测试用的假值，别用真实样本，免得又被复制出去当请求参数。
	const arqc = "test-arqc-should-never-be-a-real-cryptogram"

	client, transport := newStubClient(t, http.StatusOK, `{}`)
	client.LogRequestBody = true

	logs := captureLog(t, func() {
		if _, err := client.EMVAuthorize(context.Background(), "order-1", 100, arqc); err != nil {
			t.Fatalf("EMVAuthorize 失败: %v", err)
		}
	})

	if strings.Contains(logs, arqc) {
		t.Fatalf("日志里不应出现 ARQC 原文: %s", logs)
	}
	if !strings.Contains(logs, "len="+strconv.Itoa(len(arqc))+" sha256=") {
		t.Fatalf("日志应保留脱敏后的长度和指纹，便于判断是否被截断/破坏: %s", logs)
	}
	if !strings.Contains(logs, "transactionType") {
		t.Fatalf("非敏感字段应正常打印: %s", logs)
	}

	if !strings.Contains(string(transport.lastBody), arqc) {
		t.Fatalf("脱敏不该影响实际请求体: %s", transport.lastBody)
	}
}

func TestRequestBodyLoggingDisabledByDefault(t *testing.T) {
	client, _ := newStubClient(t, http.StatusOK, `{}`)

	logs := captureLog(t, func() {
		_, _ = client.EMVAuthorize(context.Background(), "order-1", 100, "arqc")
	})

	if logs != "" {
		t.Fatalf("默认不该打印请求体: %s", logs)
	}
}
