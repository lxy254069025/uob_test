package unigate

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"strings"
)

// sensitiveFields 是日志里需要脱敏的字段（按小写比较）。
//
// 请求体里带 ARQC、磁道、卡号、CVV 等，日志只保留长度：
// 既不影响判断"是不是空的/被截断"，又不把密文写进日志。
var sensitiveFields = map[string]bool{
	"data":           true, // encryptedData.data，ARQC 或刷卡密文
	"track1":         true,
	"track2":         true,
	"track3":         true,
	"magneprint":     true,
	"ksn":            true,
	"pan":            true,
	"cvv":            true,
	"keydata":        true,
	"applepaytoken":  true,
	"googlepaytoken": true,
}

// RedactSensitive 把 JSON 请求体里的敏感字段替换成 <redacted len=N sha256=...>。
//
// 长度能看出是否被截断，指纹能看出内容是否被破坏（例如 base64 的 + 被换成空格，
// 长度不变但内容全坏）。指纹是截断的 SHA-256，不泄露原文。
// 空字符串原样保留，这样一眼就能看出"字段没传值"。
func RedactSensitive(body []byte) string {
	var v interface{}
	if err := json.Unmarshal(body, &v); err != nil {
		// 解析不了就不要冒险打印原文。
		return "<not-json>"
	}

	// 用不转义 HTML 的编码器，否则 < > 会变成 \u003c \u003e，日志没法看。
	var buf bytes.Buffer
	enc := json.NewEncoder(&buf)
	enc.SetEscapeHTML(false)
	if err := enc.Encode(redactValue(v)); err != nil {
		return "<redact-failed>"
	}

	return strings.TrimRight(buf.String(), "\n")
}

func redactValue(v interface{}) interface{} {
	switch t := v.(type) {
	case map[string]interface{}:
		for k, val := range t {
			if s, ok := val.(string); ok && s != "" && sensitiveFields[strings.ToLower(k)] {
				t[k] = fmt.Sprintf("<redacted len=%d sha256=%s>", len(s), fingerprint(s))
				continue
			}
			t[k] = redactValue(val)
		}

		return t
	case []interface{}:
		for i, val := range t {
			t[i] = redactValue(val)
		}

		return t
	default:
		return v
	}
}

// fingerprint 返回内容摘要的前 12 个十六进制字符，用于比对两次请求是否一致。
func fingerprint(s string) string {
	sum := sha256.Sum256([]byte(s))

	return hex.EncodeToString(sum[:6])
}
