package v1

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/gin-gonic/gin"
)

func TestHomeReturnsBannersAndProducts(t *testing.T) {
	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.GET("/v1/home", Home)

	req := httptest.NewRequest(http.MethodGet, "/v1/home", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("期望 200，实际 %d: %s", w.Code, w.Body.String())
	}
	if contentType := w.Header().Get("Content-Type"); !strings.HasPrefix(contentType, "application/json") {
		t.Fatalf("Content-Type 期望 application/json，实际 %q", contentType)
	}

	// 先按原始 JSON 检查字段集合：字段名必须和 APP 端 Kotlin 数据类一致，
	// 多一个/少一个都可能导致反序列化出问题。
	var payload struct {
		Banners  []map[string]json.RawMessage `json:"banners"`
		Products []map[string]json.RawMessage `json:"products"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &payload); err != nil {
		t.Fatalf("解析响应失败: %v (body=%s)", err, w.Body.String())
	}
	if len(payload.Banners) == 0 || len(payload.Products) == 0 {
		t.Fatalf("banner 和商品都不能为空: %s", w.Body.String())
	}
	// 下面的断言依赖测试数据里正好有"无角标 banner"和"无划线价商品"。
	if len(payload.Banners) < 3 || len(payload.Products) != 16 {
		t.Fatalf("测试数据数量不符: banners=%d products=%d", len(payload.Banners), len(payload.Products))
	}

	bannerKeys := []string{"id", "title", "subtitle", "ctaText", "badgeText", "url", "imageUrl"}
	for i, banner := range payload.Banners {
		assertJSONKeys(t, "banner", i, banner, bannerKeys)

		// Kotlin 里 imageUrl 是非空 String，每条数据都必须带可用的图片地址。
		var imageURL string
		if err := json.Unmarshal(banner["imageUrl"], &imageURL); err != nil || imageURL == "" {
			t.Fatalf("banner[%d].imageUrl 期望非空字符串，实际 %s", i, banner["imageUrl"])
		}
	}

	productKeys := []string{"name", "price", "originalPrice", "off", "desc", "imageUrl"}
	for i, product := range payload.Products {
		assertJSONKeys(t, "product", i, product, productKeys)

		var imageURL string
		if err := json.Unmarshal(product["imageUrl"], &imageURL); err != nil || imageURL == "" {
			t.Fatalf("product[%d].imageUrl 期望非空字符串，实际 %s", i, product["imageUrl"])
		}

		// off 是整数百分比，客户端自己拼 "off" 后缀；0 表示没有优惠。
		var off int
		if err := json.Unmarshal(product["off"], &off); err != nil || off < 0 {
			t.Fatalf("product[%d].off 期望非负整数，实际 %s", i, product["off"])
		}
	}

	// 可空字段必须显式输出 null，而不是省略，保证 APP 侧字段始终存在。
	if got := string(payload.Banners[2]["badgeText"]); got != "null" {
		t.Fatalf("没有角标的 banner，badgeText 期望 null，实际 %s", got)
	}
	if got := string(payload.Products[1]["originalPrice"]); got != "null" {
		t.Fatalf("没有划线价的商品，originalPrice 期望 null，实际 %s", got)
	}
	if got := string(payload.Products[1]["off"]); got != "0" {
		t.Fatalf("没有优惠的商品，off 期望 0，实际 %s", got)
	}

	// 再按 Go 类型解码，确认 id/price 等字段的类型和取值符合预期。
	var response HomeResponse
	if err := json.Unmarshal(w.Body.Bytes(), &response); err != nil {
		t.Fatalf("按 HomeResponse 解析失败: %v", err)
	}
	if response.Banners[0].ID != 1 {
		t.Fatalf("首个 banner id 期望 1，实际 %d", response.Banners[0].ID)
	}
	if response.Banners[0].BadgeText == nil || *response.Banners[0].BadgeText == "" {
		t.Fatal("首个 banner 应该带角标")
	}
	if response.Products[0].Price == "" || response.Products[0].Desc == "" {
		t.Fatalf("商品价格/描述不能为空: %+v", response.Products[0])
	}
	if response.Products[0].Off != 12 {
		t.Fatalf("首个商品 off 期望 12，实际 %d", response.Products[0].Off)
	}
	if response.Products[1].Off != 0 {
		t.Fatalf("无优惠商品 off 期望 0，实际 %d", response.Products[1].Off)
	}
}

func assertJSONKeys(t *testing.T, kind string, index int, object map[string]json.RawMessage, want []string) {
	t.Helper()

	if len(object) != len(want) {
		t.Fatalf("%s[%d] 字段数量期望 %d，实际 %d: %v", kind, index, len(want), len(object), object)
	}
	for _, key := range want {
		if _, ok := object[key]; !ok {
			t.Fatalf("%s[%d] 缺少字段 %q: %v", kind, index, key, object)
		}
	}
}
