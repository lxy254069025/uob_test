package unigate

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"net/url"
	"strings"
	"time"
)

const DefaultBaseUrl = "https://svc1.magensa.net/Unigate"

type Client struct {
	BaseUrl      string
	CustomerCode string
	UserName     string
	Password     string
	HTTPClient   *http.Client

	// LogRequestBody 为 true 时把发往 Magensa 的请求体打到日志，敏感字段会脱敏。
	// 排查"ARQC 是不是空的/被截断"这类问题时就打开它。
	LogRequestBody bool
}

func NewClient(baseURL, customerCode, userName, password string) *Client {
	if baseURL == "" {
		baseURL = DefaultBaseUrl
	}
	return &Client{
		BaseUrl:      baseURL,
		CustomerCode: customerCode,
		UserName:     userName,
		Password:     password,
		HTTPClient:   &http.Client{Timeout: 30 * time.Second},
	}
}

// APIError 是 Magensa 返回非 2xx 时的错误。
type APIError struct {
	StatusCode int
	Method     string // 出错的请求方法
	Path       string // 出错的请求路径，/capture 会连调两次，用于分辨是哪一步
	Code       string // Magensa 的错误码，例如 InvalidPaymentMode / UnknownError
	Message    string // Magensa 的错误描述
	TraceID    string // Magensa 的 traceID，报障时提供给对方
	Body       string // 原始响应体，解析不出结构时兜底
}

func (e *APIError) Error() string {
	detail := e.Message
	if e.Code != "" {
		detail = e.Code
		if e.Message != "" {
			detail = e.Code + ": " + e.Message
		}
	}

	if detail == "" {
		return fmt.Sprintf("unigate: request failed with status %d: %s", e.StatusCode, e.Body)
	}

	ctx := fmt.Sprintf("status %d", e.StatusCode)
	if e.TraceID != "" {
		ctx += ", traceID " + e.TraceID
	}

	if where := strings.TrimSpace(e.Method + " " + e.Path); where != "" {
		return fmt.Sprintf("unigate: %s failed: %s (%s)", where, detail, ctx)
	}

	return fmt.Sprintf("unigate: %s (%s)", detail, ctx)
}

// faultResponse 是 Magensa 出错时返回的结构，字段比正常响应少很多。
type faultResponse struct {
	Code    string `json:"code"`
	Message string `json:"message"`
	TraceID string `json:"traceID"`
}

// parseFault 把非 2xx 的响应体解析成 APIError。
// 响应体不是 JSON（例如网关吐的 HTML）时保留原文，避免丢现场。
func parseFault(method, path string, statusCode int, body []byte) *APIError {
	err := &APIError{
		StatusCode: statusCode,
		Method:     method,
		Path:       path,
		Body:       string(body),
	}

	var fault faultResponse
	if jsonErr := json.Unmarshal(body, &fault); jsonErr == nil {
		err.Code = fault.Code
		err.Message = fault.Message
		err.TraceID = fault.TraceID
	}

	return err
}

func (c *Client) doJSON(ctx context.Context, method, path string, query url.Values, body interface{}, out interface{}) error {
	fullURL := c.BaseUrl + path
	if len(query) > 0 {
		fullURL += "?" + query.Encode()
	}

	var reqBody io.Reader
	if body != nil {
		b, err := json.Marshal(body)
		if err != nil {
			return fmt.Errorf("unigate: encoding request body: %w", err)
		}
		if c.LogRequestBody {
			log.Printf("unigate: %s %s body=%s", method, path, RedactSensitive(b))
		}
		reqBody = bytes.NewReader(b)
	}

	req, err := http.NewRequestWithContext(ctx, method, fullURL, reqBody)
	if err != nil {
		return fmt.Errorf("unigate: building request: %w", err)
	}
	req.Header.Set("Accept", "application/json")
	if body != nil {
		req.Header.Set("Content-Type", "application/json")
	}
	req.SetBasicAuth(c.CustomerCode+"/"+c.UserName, c.Password)
	resp, err := c.HTTPClient.Do(req)
	if err != nil {
		return fmt.Errorf("unigate: sending request: %w", err)
	}
	defer resp.Body.Close()
	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		return fmt.Errorf("unigate: reading response body: %w", err)
	}

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return parseFault(method, path, resp.StatusCode, respBody)
	}
	if out != nil && len(respBody) > 0 {
		if err := json.Unmarshal(respBody, out); err != nil {
			return fmt.Errorf("unigate: decoding response body: %w", err)
		}

	}
	return nil
}
