package v1

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

// Banner 首页 banner，字段与 APP 端 Kotlin 的 Banner 数据类一一对应。
type Banner struct {
	ID        int     `json:"id"`
	Title     string  `json:"title"`
	Subtitle  string  `json:"subtitle"`
	CTAText   string  `json:"ctaText"`
	BadgeText *string `json:"badgeText"`
	URL       string  `json:"url"`
	ImageURL  string  `json:"imageUrl"`
}

// Product 首页商品，字段与 APP 端 Kotlin 的 Product 数据类一一对应。
type Product struct {
	Name          string  `json:"name"`
	Price         string  `json:"price"`
	OriginalPrice *string `json:"originalPrice"`
	// Off 是优惠百分比，客户端自己拼 "off" 后缀；没有优惠时为 0。
	Off      int    `json:"off"`
	Desc     string `json:"desc"`
	ImageURL string `json:"imageUrl"`
}

// HomeResponse 首页数据：一次请求同时返回 banner 和商品，减少 APP 首屏请求数。
type HomeResponse struct {
	Banners  []Banner  `json:"banners"`
	Products []Product `json:"products"`
}

// 目前是假数据，等运营后台/商品库接入后改为从数据库读取。
// 可空字段用指针表示，序列化时会输出 null，与 Kotlin 的 String? 对应。
// 图片先用外部占位素材：banner 用 picsum，商品用透明底、无边框的 PNG（pngimg），
// APP 联调时可直接加载；上线前换成自有 CDN，并确认素材版权。
var homeBanners = []Banner{
	{
		ID:        1,
		Title:     "新客专享",
		Subtitle:  "首单立减 5 元",
		CTAText:   "立即领取",
		BadgeText: stringPtr("新人"),
		URL:       "https://example.com/activities/new-user",
		ImageURL:  "https://picsum.photos/seed/martbox-new-user/1200/480",
	},
	{
		ID:        2,
		Title:     "秋日咖啡季",
		Subtitle:  "精选咖啡第二杯半价",
		CTAText:   "去看看",
		BadgeText: stringPtr("限时"),
		URL:       "https://example.com/activities/coffee-season",
		ImageURL:  "https://picsum.photos/seed/martbox-coffee-season/1200/480",
	},
	{
		ID:       3,
		Title:    "关于我们",
		Subtitle: "了解 martbox",
		CTAText:  "查看详情",
		// BadgeText 为空时不展示角标，APP 需要处理 null。
		BadgeText: nil,
		// URL 为空表示该 banner 没有跳转目标。
		URL:      "",
		ImageURL: "https://picsum.photos/seed/martbox-about/1200/480",
	},
}

// homeProductCount 是首页一次返回的商品数，16 条方便 APP 联调 4x4 宫格。
const homeProductCount = 16

// 基础商品只有 3 条，按顺序循环填充到 homeProductCount 条。
var homeProducts = repeatProducts([]Product{
	{
		Name:          "冰镇可乐 330ml",
		Price:         "3.50",
		OriginalPrice: stringPtr("4.00"),
		// (4.00-3.50)/4.00 = 12.5%，向下取整为 12；"off" 由客户端拼接。
		Off:      12,
		Desc:     "经典口味，冰镇后更好喝",
		ImageURL: "https://pngimg.com/uploads/cocacola/coca_cola_PNG8909.png",
	},
	{
		Name:  "现磨美式咖啡",
		Price: "12.00",
		// 没有划线价，off 为 0，客户端据此不展示优惠。
		OriginalPrice: nil,
		Off:           0,
		Desc:          "中深烘焙，现点现做",
		ImageURL:      "https://pngimg.com/uploads/mug_coffee/mug_coffee_PNG97402.png",
	},
	{
		Name:          "蜂蜜烧烤味薯片",
		Price:         "6.50",
		OriginalPrice: stringPtr("7.50"),
		// (7.50-6.50)/7.50 ≈ 13.3%，向下取整为 13。
		Off:      13,
		Desc:     "甜辣烧烤风味，酥脆不油腻",
		ImageURL: "https://pngimg.com/uploads/potato_chips/potato_chips_PNG43.png",
	},
}, homeProductCount)

func repeatProducts(base []Product, count int) []Product {
	if len(base) == 0 || count <= 0 {
		return nil
	}

	products := make([]Product, 0, count)
	for i := 0; i < count; i++ {
		products = append(products, base[i%len(base)])
	}
	return products
}

// Home 返回 APP 首页的 banner 与商品数据。
//
// 目前没有分页、没有鉴权，也不区分设备/门店；接入运营系统后需要按设备所属
// 门店或城市过滤，并补上缓存。
func Home(c *gin.Context) {
	c.JSON(http.StatusOK, HomeResponse{
		Banners:  homeBanners,
		Products: homeProducts,
	})
}

func stringPtr(value string) *string {
	return &value
}
