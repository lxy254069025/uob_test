package v1

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"uopenbox/common/unigate"

	"github.com/gin-gonic/gin"
)

func newTestContext(t *testing.T) (*gin.Context, *httptest.ResponseRecorder) {
	t.Helper()

	gin.SetMode(gin.TestMode)
	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest(http.MethodPost, "/v1/transaction/emv", strings.NewReader(""))

	return c, w
}

func decodeBody(t *testing.T, w *httptest.ResponseRecorder) map[string]string {
	t.Helper()

	var body map[string]string
	if err := json.Unmarshal(w.Body.Bytes(), &body); err != nil {
		t.Fatalf("解析响应失败: %v (body=%s)", err, w.Body.String())
	}

	return body
}

func TestRespondUnigateErrorMapsUpstream4xxToBadRequest(t *testing.T) {
	g, w := newTestContext(t)

	respondUnigateError(g, &unigate.Response{
		StatusCode:   http.StatusBadRequest,
		FaultCode:    "InvalidPaymentMode",
		FaultMessage: "PaymentMode is not supported - Invalid value [] for tag DFDF52 in ARQC",
		FaultTraceID: "c1d35cdb-0a36-4571-9d7c-83464fd468fe",
	})

	if w.Code != http.StatusBadRequest {
		t.Fatalf("期望 400，实际 %d", w.Code)
	}
	body := decodeBody(t, w)
	if body["error"] != "InvalidPaymentMode" {
		t.Fatalf("error 应为 Magensa 的 code，实际 %q", body["error"])
	}
	if !strings.Contains(body["message"], "DFDF52") {
		t.Fatalf("message 应透出上游描述，实际 %q", body["message"])
	}
	if body["traceID"] != "c1d35cdb-0a36-4571-9d7c-83464fd468fe" {
		t.Fatalf("traceID 应透出给设备报障用，实际 %q", body["traceID"])
	}
}

func TestRespondUnigateErrorMapsUpstream5xxToBadGateway(t *testing.T) {
	g, w := newTestContext(t)

	respondUnigateError(g, &unigate.Response{
		StatusCode:   http.StatusInternalServerError,
		FaultCode:    "UnknownError",
		FaultTraceID: "ad525361-a643-4fbc-981a-5117b3279b19",
	})

	if w.Code != http.StatusBadGateway {
		t.Fatalf("期望 502，实际 %d", w.Code)
	}
	if body := decodeBody(t, w); body["error"] != "UnknownError" {
		t.Fatalf("error 应为 Magensa 的 code，实际 %q", body["error"])
	}
}

// 请求只带 paymentType 和 data；EMV 授权格式由 C SDK 根据 operation 生成。
func TestEmvAuthorizeRequestFormat(t *testing.T) {
	got := emvAuthorizeRequest("order-1", "", "card-data")

	if got.Operation != unigate.OperationEMV {
		t.Fatalf("operation 应为 EMV，实际 %d", got.Operation)
	}
	if got.DataType != dataTypeARQC {
		t.Fatalf("dataType 应固定为 ARQC，实际 %q", got.DataType)
	}
	if got.TransactionType != txnTypeAuthorize {
		t.Fatalf("transactionType 应为 AUTHORIZE，实际 %q", got.TransactionType)
	}
	if got.Amount != emvAuthorizeAmount {
		t.Fatalf("金额应由方法自己决定，期望 %v，实际 %v", emvAuthorizeAmount, got.Amount)
	}
	if got.PaymentType != paymentCredit {
		t.Fatalf("paymentType 默认应为 Credit，实际 %q", got.PaymentType)
	}
	if got.Data != "card-data" || got.CustomerTransactionID != "order-1" {
		t.Fatalf("请求字段不正确: %+v", got)
	}
}

func TestEmvAuthorizeRequestPassesPaymentType(t *testing.T) {
	got := emvAuthorizeRequest("order-1", "  Debit  ", "card-data")
	if got.PaymentType != "Debit" {
		t.Fatalf("paymentType 应去掉空格后透传，实际 %q", got.PaymentType)
	}
}

// 重放意味着这笔没有真的发起新授权，必须能识别出来。
func TestReplayNotice(t *testing.T) {
	if notice, ok := replayNotice(nil); ok || notice != "" {
		t.Fatalf("空响应不该报重放: %q", notice)
	}

	normal := []byte(`{"magTranID":"m1"}`)
	if notice, ok := replayNotice(normal); ok {
		t.Fatalf("正常交易不该报重放: %q", notice)
	}

	replayed := []byte(`{"magTranID":"m2","customerTransactionID":"cst-ar-735",` +
		`"dataOutput":{"isReplay":true,"panLast4":"4111"},` +
		`"transactionOutput":{"isTransactionApproved":true}}`)
	notice, ok := replayNotice(replayed)
	if !ok {
		t.Fatal("isReplay=true 应报重放")
	}
	for _, want := range []string{"cst-ar-735", "m2", "4111", "授权=true"} {
		if !strings.Contains(notice, want) {
			t.Fatalf("告警信息缺少 %q: %s", want, notice)
		}
	}
}

func TestEmvRejectsBadRequest(t *testing.T) {
	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.POST("/v1/transaction/emv", Emv)

	cases := map[string]string{
		"空字符串":    `{"data":""}`,
		"全空格":     `{"data":"   "}`,
		"缺少字段":    `{"paymentType":"Credit"}`,
		"非法 JSON": `{`,
	}

	for name, body := range cases {
		t.Run(name, func(t *testing.T) {
			req := httptest.NewRequest(http.MethodPost, "/v1/transaction/emv", strings.NewReader(body))
			req.Header.Set("Content-Type", "application/json")

			w := httptest.NewRecorder()
			r.ServeHTTP(w, req)

			if w.Code != http.StatusBadRequest {
				t.Fatalf("期望 400，实际 %d: %s", w.Code, w.Body.String())
			}
		})
	}
}

func TestCaptureRejectsEmptyReferenceMagTran(t *testing.T) {
	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.POST("/v1/transaction/capture", Capture)

	req := httptest.NewRequest(http.MethodPost, "/v1/transaction/capture", strings.NewReader(`{"reference_mag_tran":"  "}`))
	req.Header.Set("Content-Type", "application/json")

	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	if w.Code != http.StatusBadRequest {
		t.Fatalf("期望 400，实际 %d: %s", w.Code, w.Body.String())
	}
}
