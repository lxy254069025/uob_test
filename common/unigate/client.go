package unigate

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"time"
)

const DefaultBaseUrl = "https://svc1.magensa.net/Unigate"

type Client struct {
	BaseUrl      string
	CustomerCode string
	UserName     string
	Password     string
	HTTPClient   *http.Client
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

type APIError struct {
	StatusCode int
	Message    string
	Body       string
}

func (e *APIError) Error() string {
	if e.Message != "" {
		return fmt.Sprintf("unigate: %s (status %d)", e.Message, e.StatusCode)
	}

	return fmt.Sprintf("unigate: request failed with status %d: %s", e.StatusCode, e.Body)
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
		return &APIError{
			StatusCode: resp.StatusCode,
			Body:       string(respBody),
		}
	}
	if out != nil && len(respBody) > 0 {
		if err := json.Unmarshal(respBody, out); err != nil {
			return fmt.Errorf("unigate: decoding response body: %w", err)
		}

	}
	return nil
}
