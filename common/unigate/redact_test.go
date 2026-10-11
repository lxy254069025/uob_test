package unigate

import (
	"encoding/json"
	"strings"
	"testing"
)

// EMV 请求的真实结构：嵌套两层，ARQC 在 dataInput.encryptedData.data 里。
func TestRedactSensitiveMasksNestedARQC(t *testing.T) {
	raw := []byte(`{"customerTransactionID":"order-1",` +
		`"dataInput":{"encryptedData":{"dataType":"ARQC","data":"SECRET-ARQC-VALUE"},"paymentType":"Credit"}}`)

	got := RedactSensitive(raw)

	if strings.Contains(got, "SECRET-ARQC-VALUE") {
		t.Fatalf("ARQC 原文不应出现在日志里: %s", got)
	}
	if !strings.Contains(got, "len=17 sha256=") {
		t.Fatalf("应替换成带长度和指纹的占位符: %s", got)
	}
	if !strings.Contains(got, "<redacted ") || strings.Contains(got, `\u003c`) {
		t.Fatalf("占位符应保持可读，不能被 JSON 转义: %s", got)
	}
	if !strings.Contains(got, `"dataType":"ARQC"`) || !strings.Contains(got, `"customerTransactionID":"order-1"`) {
		t.Fatalf("非敏感字段应原样保留: %s", got)
	}

	if !json.Valid([]byte(got)) {
		t.Fatalf("脱敏结果应仍是合法 JSON: %s", got)
	}
}

// 空值原样保留，这样一眼能看出"字段没传值"。
func TestRedactSensitiveKeepsEmptyValues(t *testing.T) {
	got := RedactSensitive([]byte(`{"encryptedData":{"data":""}}`))
	if !strings.Contains(got, `"data":""`) {
		t.Fatalf("空值应原样保留，实际 %s", got)
	}
}

func TestRedactSensitiveMasksCardFields(t *testing.T) {
	raw := []byte(`{"encryptedCardSwipe":{"track1":"T1SECRET","track2":"T2SECRET","magnePrint":"MP","ksn":"KSN001"},` +
		`"manualEntryInput":{"pan":"4111111111111111","cvv":"123"}}`)

	got := RedactSensitive(raw)

	for _, secret := range []string{"T1SECRET", "T2SECRET", "MP", "KSN001", "4111111111111111", `"123"`} {
		if strings.Contains(got, secret) {
			t.Fatalf("敏感值 %s 不应出现在日志里: %s", secret, got)
		}
	}
}

func TestRedactSensitiveHandlesArrays(t *testing.T) {
	got := RedactSensitive([]byte(`{"entries":[{"track2":"T2SECRET"},{"track2":""}]}`))

	if strings.Contains(got, "T2SECRET") {
		t.Fatalf("数组里的敏感值也要脱敏: %s", got)
	}
	if !strings.Contains(got, "len=8 sha256=") || !strings.Contains(got, `"track2":""`) {
		t.Fatalf("数组元素应分别处理: %s", got)
	}
}

// 长度相同但内容被破坏（例如 base64 的 + 变成空格）时，只能靠指纹发现。
func TestRedactSensitiveFingerprintDetectsSameLengthChange(t *testing.T) {
	original := RedactSensitive([]byte(`{"encryptedData":{"data":"abc+def"}}`))
	sameContent := RedactSensitive([]byte(`{"encryptedData":{"data":"abc+def"}}`))
	corrupted := RedactSensitive([]byte(`{"encryptedData":{"data":"abc def"}}`))

	if original != sameContent {
		t.Fatalf("同样的内容应给出同样的指纹: %s vs %s", original, sameContent)
	}
	if original == corrupted {
		t.Fatalf("长度相同但内容不同，指纹必须不同: %s", original)
	}
	if !strings.Contains(original, "len=7") || !strings.Contains(corrupted, "len=7") {
		t.Fatalf("两者的长度应都是 7: %s / %s", original, corrupted)
	}
}

// 解析不了的报文不要冒险打印原文。
func TestRedactSensitiveRejectsNonJSON(t *testing.T) {
	if got := RedactSensitive([]byte("<html>bank error</html>")); got != "<not-json>" {
		t.Fatalf("非 JSON 应返回占位符，实际 %s", got)
	}
}
