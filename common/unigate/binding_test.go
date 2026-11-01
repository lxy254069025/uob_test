//go:build cgo && (darwin || linux) && (amd64 || arm64)

package unigate

import (
	"bytes"
	"encoding/pem"
	"fmt"
	"io"
	"log"
	"net/http"
	"net/http/httptest"
	"os"
	"strconv"
	"strings"
	"sync"
	"testing"
	"time"
)

var testTLSServer *httptest.Server

func TestMain(m *testing.M) {
	testTLSServer = httptest.NewUnstartedServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		_, _ = w.Write([]byte(`{"magTranID":"TLS123","transactionOutput":{"isTransactionApproved":true}}`))
	}))
	testTLSServer.StartTLS()

	caFile, err := os.CreateTemp("", "unigate-test-ca-*.pem")
	if err != nil {
		fmt.Fprintf(os.Stderr, "创建测试 CA 文件失败: %v\n", err)
		os.Exit(1)
	}
	if err := pem.Encode(caFile, &pem.Block{
		Type:  "CERTIFICATE",
		Bytes: testTLSServer.Certificate().Raw,
	}); err != nil {
		fmt.Fprintf(os.Stderr, "写入测试 CA 文件失败: %v\n", err)
		os.Exit(1)
	}
	if err := caFile.Close(); err != nil {
		fmt.Fprintf(os.Stderr, "关闭测试 CA 文件失败: %v\n", err)
		os.Exit(1)
	}
	if err := os.Setenv("UNIGATE_CA_BUNDLE", caFile.Name()); err != nil {
		fmt.Fprintf(os.Stderr, "设置 UNIGATE_CA_BUNDLE 失败: %v\n", err)
		os.Exit(1)
	}

	code := m.Run()
	testTLSServer.Close()
	_ = os.Remove(caFile.Name())
	os.Exit(code)
}

type stubServer struct {
	status     int
	body       string
	mu         sync.Mutex
	lastBody   []byte
	lastMethod string
	lastPath   string
	lastAuth   string
}

func newStub(t *testing.T, status int, body string) (ClientOptions, *stubServer) {
	t.Helper()

	stub := &stubServer{status: status, body: body}
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		requestBody, err := io.ReadAll(r.Body)
		if err != nil {
			t.Errorf("读取请求体失败: %v", err)
		}

		stub.mu.Lock()
		stub.lastBody = requestBody
		stub.lastMethod = r.Method
		stub.lastPath = r.URL.RequestURI()
		stub.lastAuth = r.Header.Get("Authorization")
		stub.mu.Unlock()

		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(status)
		_, _ = w.Write([]byte(body))
	}))
	t.Cleanup(server.Close)

	return ClientOptions{
		BaseURL:      server.URL + "/Unigate",
		CustomerCode: "CODE",
		UserName:     "USER",
		Password:     "PASS",
	}, stub
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

func TestFaultResultMatchesGoShape(t *testing.T) {
	options, _ := newStub(t, http.StatusBadRequest,
		`{"traceID":"trace-1","code":"InvalidPaymentMode","message":"bad ARQC"}`)

	response, err := Execute(options, Request{
		Operation:             OperationEMV,
		CustomerTransactionID: "order-1",
		TransactionType:       "AUTHORIZE",
		Amount:                100,
		DataType:              "ARQC",
		Data:                  "arqc",
		PaymentType:           "Credit",
	})
	if err != nil {
		t.Fatalf("Execute 失败: %v", err)
	}
	if response.StatusCode != http.StatusBadRequest ||
		response.FaultCode != "InvalidPaymentMode" ||
		response.FaultMessage != "bad ARQC" ||
		response.FaultTraceID != "trace-1" {
		t.Fatalf("fault 结果不正确: %+v", response)
	}
}

func TestRequestBodyLoggingRedactsARQC(t *testing.T) {
	const arqc = "test-arqc-should-never-be-a-real-cryptogram"
	options, stub := newStub(t, http.StatusOK, `{}`)
	options.LogRequestBody = true

	logs := captureLog(t, func() {
		if _, err := Execute(options, Request{
			Operation:             OperationEMV,
			CustomerTransactionID: "order-1",
			TransactionType:       "AUTHORIZE",
			Amount:                100,
			DataType:              "ARQC",
			Data:                  arqc,
			PaymentType:           "Credit",
		}); err != nil {
			t.Fatalf("Execute 失败: %v", err)
		}
	})

	if strings.Contains(logs, arqc) ||
		!strings.Contains(logs, "len="+strconv.Itoa(len(arqc))+" sha256=") ||
		!strings.Contains(logs, "AUTHORIZE") {
		t.Fatalf("脱敏日志不正确: %s", logs)
	}
	stub.mu.Lock()
	defer stub.mu.Unlock()
	if !strings.Contains(string(stub.lastBody), arqc) {
		t.Fatalf("脱敏不应影响实际请求: %s", stub.lastBody)
	}
}

func TestRequestBodyLoggingDisabledByDefault(t *testing.T) {
	options, _ := newStub(t, http.StatusOK, `{}`)
	logs := captureLog(t, func() {
		_, _ = Execute(options, Request{
			Operation:             OperationToken,
			CustomerTransactionID: "order-1",
			TransactionType:       "SALE",
			Token:                 "token",
		})
	})
	if logs != "" {
		t.Fatalf("默认不应输出请求体: %s", logs)
	}
}

func TestHTTPSUsesBundledMbedTLS(t *testing.T) {
	response, err := Execute(ClientOptions{
		BaseURL:      testTLSServer.URL + "/Unigate",
		CustomerCode: "CODE",
		UserName:     "USER",
		Password:     "PASS",
		Timeout:      5 * time.Second,
	}, Request{
		Operation:             OperationEMV,
		CustomerTransactionID: "order-tls",
		TransactionType:       "AUTHORIZE",
		Amount:                100,
		DataType:              "ARQC",
		Data:                  "arqc",
		PaymentType:           "Credit",
	})
	if err != nil {
		t.Fatalf("HTTPS 请求失败: %v", err)
	}
	if !strings.Contains(string(response.Body), `"magTranID":"TLS123"`) {
		t.Fatalf("HTTPS 响应不正确: %s", response.Body)
	}
}

func TestExecuteReturnsTransportError(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(http.ResponseWriter, *http.Request) {}))
	baseURL := server.URL + "/Unigate"
	server.Close()

	_, err := Execute(ClientOptions{
		BaseURL:      baseURL,
		CustomerCode: "CODE",
		UserName:     "USER",
		Password:     "PASS",
		Timeout:      time.Second,
	}, Request{Operation: OperationToken, Token: "token"})
	if err == nil {
		t.Fatal("连接失败时 Execute 应返回错误")
	}
}

func TestAllOperationsAreBuiltByCSDK(t *testing.T) {
	tests := []struct {
		name         string
		request      Request
		wantMethod   string
		wantPath     string
		bodyContains []string
	}{
		{
			name:         "token",
			request:      Request{Operation: OperationToken, CustomerTransactionID: "order-token", TransactionType: "SALE", Amount: 12.34, Token: "card-token"},
			wantMethod:   http.MethodPost,
			wantPath:     "/Unigate/api/Transaction/Token",
			bodyContains: []string{`"token":"card-token"`, `"amount":12.34`},
		},
		{
			name:         "card swipe",
			request:      Request{Operation: OperationCardSwipe, CustomerTransactionID: "order-swipe", TransactionType: "SALE", Amount: 20, Track2: "track-2", KSN: "ksn-1", PaymentType: "Credit", IsFallback: "true"},
			wantMethod:   http.MethodPost,
			wantPath:     "/Unigate/api/Transaction/CardSwipe",
			bodyContains: []string{`"track2":"track-2"`, `"isFallBack":"true"`},
		},
		{
			name:         "keypad",
			request:      Request{Operation: OperationKeypad, CustomerTransactionID: "order-keypad", TransactionType: "SALE", Amount: 30, Track2: "pin-block", KSN: "ksn-2", PaymentType: "Implicit"},
			wantMethod:   http.MethodPost,
			wantPath:     "/Unigate/api/Transaction/KeyPadEntry",
			bodyContains: []string{`"track2":"pin-block"`, `"paymentType":"Implicit"`},
		},
		{
			name:         "manual",
			request:      Request{Operation: OperationManual, CustomerTransactionID: "order-manual", TransactionType: "SALE", Amount: 40, PAN: "4111111111111111", ExpirationDate: "12/30", CVV: "123"},
			wantMethod:   http.MethodPost,
			wantPath:     "/Unigate/api/Transaction/ManualEntry",
			bodyContains: []string{`"pan":"4111111111111111"`, `"expirationDate":"12/30"`},
		},
		{
			name:         "in app apple pay",
			request:      Request{Operation: OperationInAppApplePay, CustomerTransactionID: "order-apple", TransactionType: "SALE", Amount: 50, ApplePayToken: "apple-token"},
			wantMethod:   http.MethodPost,
			wantPath:     "/Unigate/api/Transaction/InAppApplePay",
			bodyContains: []string{`"applePayToken":"apple-token"`},
		},
		{
			name:         "tec apple pay",
			request:      Request{Operation: OperationTECApplePay, CustomerTransactionID: "order-tec", TransactionType: "SALE", Amount: 60, ApplePayToken: "tec-token"},
			wantMethod:   http.MethodPost,
			wantPath:     "/Unigate/api/Transaction/TECApplePay",
			bodyContains: []string{`"applePayToken":"tec-token"`},
		},
		{
			name:         "google pay",
			request:      Request{Operation: OperationGooglePay, CustomerTransactionID: "order-google", TransactionType: "SALE", Amount: 70, GooglePayToken: "google-token"},
			wantMethod:   http.MethodPost,
			wantPath:     "/Unigate/api/Transaction/GooglePay",
			bodyContains: []string{`"googlePayToken":"google-token"`},
		},
		{
			name:         "incremental authorize",
			request:      Request{Operation: OperationIncrementalAuthorize, ReferenceMagTranID: "mag-1", Amount: 80},
			wantMethod:   http.MethodPost,
			wantPath:     "/Unigate/api/ReferenceTransaction/AUTHORIZE",
			bodyContains: []string{`"referenceMagTranID":"mag-1"`, `"amount":80`},
		},
		{
			name: "tip adjust",
			request: Request{
				Operation:               OperationTipAdjust,
				ReferenceMagTranID:      "mag-2",
				TipAmount:               5,
				TransactionInputDetails: []KeyValue{{Key: "tipType", Value: "percent"}},
			},
			wantMethod:   http.MethodPost,
			wantPath:     "/Unigate/api/ReferenceTransaction/TIPADJUST",
			bodyContains: []string{`"tipAmount":5`, `"transactionInputDetails":[{"key":"tipType","value":"percent"}]`},
		},
		{
			name:         "capture",
			request:      Request{Operation: OperationCapture, ReferenceMagTranID: "mag-3", Amount: 90},
			wantMethod:   http.MethodPost,
			wantPath:     "/Unigate/api/ReferenceTransaction/CAPTURE",
			bodyContains: []string{`"referenceMagTranID":"mag-3"`, `"amount":90`},
		},
		{
			name:         "refund",
			request:      Request{Operation: OperationRefund, ReferenceMagTranID: "mag-4", Amount: 10},
			wantMethod:   http.MethodPost,
			wantPath:     "/Unigate/api/ReferenceTransaction/REFUND",
			bodyContains: []string{`"referenceMagTranID":"mag-4"`, `"amount":10`},
		},
		{
			name:         "full void",
			request:      Request{Operation: OperationVoid, ReferenceMagTranID: "mag-5"},
			wantMethod:   http.MethodPost,
			wantPath:     "/Unigate/api/ReferenceTransaction/VOID",
			bodyContains: []string{`"referenceMagTranID":"mag-5"`},
		},
		{
			name:         "partial void",
			request:      Request{Operation: OperationVoid, ReferenceMagTranID: "mag-6", Amount: 6.5, HasAmount: true},
			wantMethod:   http.MethodPost,
			wantPath:     "/Unigate/api/ReferenceTransaction/VOID",
			bodyContains: []string{`"referenceMagTranID":"mag-6"`, `"amount":6.5`},
		},
		{
			name:       "get by mag tran id",
			request:    Request{Operation: OperationGetByMagTranID, MagTranID: "mag/id ?"},
			wantMethod: http.MethodGet,
			wantPath:   "/Unigate/api/Transaction/mag%2Fid%20%3F",
		},
		{
			name:       "get by customer tran id",
			request:    Request{Operation: OperationGetByCustomerTranID, CustomerTransactionQuery: "order/id ?"},
			wantMethod: http.MethodGet,
			wantPath:   "/Unigate/api/Transaction?custTranID=order%2Fid%20%3F",
		},
		{
			name:         "apple tap to pay reader",
			request:      Request{Operation: OperationAppleTapToPayReader, ReaderIdentifier: "reader-1"},
			wantMethod:   http.MethodPost,
			wantPath:     "/Unigate/api/AppleTapToPayToken/PaymentCardReader",
			bodyContains: []string{`"paymentCardReaderIdentifier":"reader-1"`},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			options, stub := newStub(t, http.StatusOK, `{}`)
			if _, err := Execute(options, test.request); err != nil {
				t.Fatalf("Execute 失败: %v", err)
			}

			stub.mu.Lock()
			defer stub.mu.Unlock()
			if stub.lastMethod != test.wantMethod || stub.lastPath != test.wantPath {
				t.Fatalf("请求行不正确: got %s %s, want %s %s", stub.lastMethod, stub.lastPath, test.wantMethod, test.wantPath)
			}
			for _, fragment := range test.bodyContains {
				if !strings.Contains(string(stub.lastBody), fragment) {
					t.Fatalf("请求体缺少 %q: %s", fragment, stub.lastBody)
				}
			}
			if test.wantMethod == http.MethodGet && len(stub.lastBody) != 0 {
				t.Fatalf("GET 请求不应有 body: %s", stub.lastBody)
			}
		})
	}
}
