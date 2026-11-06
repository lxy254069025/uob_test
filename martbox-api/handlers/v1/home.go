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
	DiscountLabel *string `json:"discountLabel"`
	Desc          string  `json:"desc"`
	ImageURL      string  `json:"imageUrl"`
}

// HomeResponse 首页数据：一次请求同时返回 banner 和商品，减少 APP 首屏请求数。
type HomeResponse struct {
	Banners  []Banner  `json:"banners"`
	Products []Product `json:"products"`
}

// 目前是假数据，等运营后台/商品库接入后改为从数据库读取。
// 可空字段用指针表示，序列化时会输出 null，与 Kotlin 的 String? 对应。
// 图片地址先用 picsum.photos 占位，APP 联调时能直接加载；接入后台后换成 CDN 地址。
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

var homeProducts = []Product{
	{
		Name:          "冰镇可乐 330ml",
		Price:         "3.50",
		OriginalPrice: stringPtr("4.00"),
		DiscountLabel: stringPtr("限时特价"),
		Desc:          "经典口味，冰镇后更好喝",
		ImageURL:      "https://picsum.photos/seed/martbox-cola/600/600",
	},
	{
		Name:  "现磨美式咖啡",
		Price: "12.00",
		// 无划线价、无优惠标签时输出 null。
		OriginalPrice: nil,
		DiscountLabel: nil,
		Desc:          "中深烘焙，现点现做",
		ImageURL:      "https://picsum.photos/seed/martbox-americano/600/600",
	},
	{
		Name:          "原味薯片 70g",
		Price:         "6.50",
		OriginalPrice: stringPtr("7.50"),
		DiscountLabel: stringPtr("会员价"),
		Desc:          "酥脆轻薄，追剧必备",
		ImageURL:      "https://picsum.photos/seed/martbox-chips/600/600",
	},
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
