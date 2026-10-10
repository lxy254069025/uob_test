package v1

import (
	"encoding/json"
	"errors"
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

// Magensa 的 4xx 说明我们送上去的数据有问题，对设备来说就是请求错误。
func TestRespondUnigateErrorMapsUpstream4xxToBadRequest(t *testing.T) {
	g, w := newTestContext(t)

	respondUnigateError(g, &unigate.APIError{
		StatusCode: http.StatusBadRequest,
		Code:       "InvalidPaymentMode",
		Message:    "PaymentMode is not supported - Invalid value [] for tag DFDF52 in ARQC",
		TraceID:    "c1d35cdb-0a36-4571-9d7c-83464fd468fe",
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

// 上游 5xx 是对方故障，对我们来说就是 502。
func TestRespondUnigateErrorMapsUpstream5xxToBadGateway(t *testing.T) {
	g, w := newTestContext(t)

	respondUnigateError(g, &unigate.APIError{
		StatusCode: http.StatusInternalServerError,
		Code:       "UnknownError",
		TraceID:    "ad525361-a643-4fbc-981a-5117b3279b19",
	})

	if w.Code != http.StatusBadGateway {
		t.Fatalf("期望 502，实际 %d", w.Code)
	}
	if body := decodeBody(t, w); body["error"] != "UnknownError" {
		t.Fatalf("error 应为 Magensa 的 code，实际 %q", body["error"])
	}
}

func TestRespondUnigateErrorHandlesPlainError(t *testing.T) {
	g, w := newTestContext(t)

	respondUnigateError(g, errors.New("unigate: sending request: dial tcp: timeout"))

	if w.Code != http.StatusBadGateway {
		t.Fatalf("期望 502，实际 %d", w.Code)
	}
	if body := decodeBody(t, w); body["error"] != "upstream_error" {
		t.Fatalf("非 APIError 应回落到 upstream_error，实际 %q", body["error"])
	}
}

// 请求只带 paymentType 和 data：EMV 授权方法的报文格式由自己组装。
func TestEmvAuthorizeRequestFormat(t *testing.T) {
	got := emvAuthorizeRequest("order-1", "", "card-data")

	if got.DataInput.EncryptedData.DataType != unigate.DataTypeARQC {
		t.Fatalf("EMV 授权方法的 dataType 应固定为 ARQC，实际 %q", got.DataInput.EncryptedData.DataType)
	}
	if got.TransactionInput.TransactionType != unigate.TxnTypeAuthorize {
		t.Fatalf("transactionType 应为 AUTHORIZE，实际 %q", got.TransactionInput.TransactionType)
	}
	if got.TransactionInput.Amount != emvAuthorizeAmount {
		t.Fatalf("金额应由方法自己决定，期望 %v，实际 %v", emvAuthorizeAmount, got.TransactionInput.Amount)
	}
	if got.DataInput.PaymentType != unigate.PaymentTypeCredit {
		t.Fatalf("paymentType 默认应为 Credit，实际 %q", got.DataInput.PaymentType)
	}
	if got.DataInput.EncryptedData.Data != "card-data" {
		t.Fatalf("data 应原样带上: %+v", got)
	}
	if got.CustomerTransactionID != "order-1" {
		t.Fatalf("customerTransactionID 没带上: %+v", got)
	}
}

func TestEmvAuthorizeRequestPassesPaymentType(t *testing.T) {
	got := emvAuthorizeRequest("order-1", "  Debit  ", "card-data")

	if got.DataInput.PaymentType != "Debit" {
		t.Fatalf("paymentType 应原样透传并去掉空格，实际 %q", got.DataInput.PaymentType)
	}
}

// 重放意味着这笔没有真的发起新授权，必须能识别出来。
func TestReplayNotice(t *testing.T) {
	if notice, ok := replayNotice(nil); ok || notice != "" {
		t.Fatalf("nil 响应不该报重放: %q", notice)
	}

	normal := &unigate.TransactionResponse{MagTranID: "m1"}
	if notice, ok := replayNotice(normal); ok {
		t.Fatalf("正常交易不该报重放: %q", notice)
	}

	replayed := &unigate.TransactionResponse{
		MagTranID:             "m2",
		CustomerTransactionID: "cst-ar-735",
		DataOutput:            unigate.DataOutput{IsReplay: true, PanLast4: "4111"},
		TransactionOutput:     unigate.TransactionOutput{IsTransactionApproved: true},
	}
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

// 空数据在入口就该被拦下，不能送给 Magensa 换回一个语焉不详的 UnknownError。
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
