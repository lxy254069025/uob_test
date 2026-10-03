package v1

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"uopenbox/martbox-api/config"
	"uopenbox/martbox-api/token"

	"github.com/gin-gonic/gin"
)

const testIMEI = "860000000000001"

func authorRouter(t *testing.T) *gin.Engine {
	t.Helper()

	// 用真实的 home.yaml，顺带保证配置项没写错。
	config.InitConfig("../../etc/home.yaml")

	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.POST("/author", Author)

	return r
}

func postAuthor(t *testing.T, r *gin.Engine, body string) *httptest.ResponseRecorder {
	t.Helper()

	req := httptest.NewRequest(http.MethodPost, "/author", strings.NewReader(body))
	req.Header.Set("Content-Type", "application/json")

	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	return w
}

func TestAuthorReturnsTokenForIMEI(t *testing.T) {
	r := authorRouter(t)

	w := postAuthor(t, r, `{"imei":"`+testIMEI+`","sn":"SN-0001"}`)
	if w.Code != http.StatusOK {
		t.Fatalf("期望 200，实际 %d: %s", w.Code, w.Body.String())
	}

	var resp AuthorResponse
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil {
		t.Fatalf("解析响应失败: %v", err)
	}
	if resp.IMEI != testIMEI {
		t.Fatalf("imei 期望 %s，实际 %s", testIMEI, resp.IMEI)
	}
	if resp.ExpiresIn <= 0 || resp.ExpiresAt <= time.Now().Unix() {
		t.Fatalf("有效期不符合预期: %+v", resp)
	}

	// 用配置里的密钥反解 token，确认载荷确实是这个 imei。
	auth := config.GetConfig().Auth
	claims, err := token.NewSigner(auth.Secret, auth.Issuer, 0).Parse(resp.Token)
	if err != nil {
		t.Fatalf("签发的 token 无法校验: %v", err)
	}
	if claims.IMEI != testIMEI {
		t.Fatalf("token 里的 imei 期望 %s，实际 %s", testIMEI, claims.IMEI)
	}
	if want := time.Duration(auth.Expire) * time.Second; claims.ExpiresAt.Time.Sub(time.Now()) > want {
		t.Fatalf("token 有效期超过了配置: %v", claims.ExpiresAt.Time)
	}
}

func TestAuthorTrimsIMEI(t *testing.T) {
	r := authorRouter(t)

	w := postAuthor(t, r, `{"imei":"  `+testIMEI+`  "}`)
	if w.Code != http.StatusOK {
		t.Fatalf("期望 200，实际 %d: %s", w.Code, w.Body.String())
	}

	var resp AuthorResponse
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil {
		t.Fatalf("解析响应失败: %v", err)
	}
	if resp.IMEI != testIMEI {
		t.Fatalf("imei 应去掉首尾空格，实际 %q", resp.IMEI)
	}
}

func TestAuthorRejectsBadRequest(t *testing.T) {
	r := authorRouter(t)

	cases := map[string]string{
		"缺少 imei":  `{"sn":"SN-0001"}`,
		"imei 为空":  `{"imei":""}`,
		"imei 全空格": `{"imei":"   "}`,
		"非法 JSON":  `{`,
	}

	for name, body := range cases {
		t.Run(name, func(t *testing.T) {
			if got := postAuthor(t, r, body).Code; got != http.StatusBadRequest {
				t.Fatalf("期望 400，实际 %d", got)
			}
		})
	}
}
