package middleware

// import (
// 	"fmt"
// 	"yf_life/api/home/config"
// 	"yf_life/api/services"
// 	"yf_life/common/errorx"
// 	"yf_life/common/httpx"
// 	"yf_life/common/utils"

// 	"github.com/gin-gonic/gin"
// )

// func CheckToken() gin.HandlerFunc {
// 	return func(c *gin.Context) {
// 		//从请求头中获取token
// 		tokenStr := c.Request.Header.Get("Authorization")
// 		//用户不存在
// 		if tokenStr == "" {
// 			c.Abort() //阻止执行
// 			httpx.SendError(c, errorx.NewCodeError(401, "request header Authorization formal error 1"))
// 			return
// 		}

// 		//验证token
// 		mc, err := utils.DeCodeJwtToken(tokenStr, "6bb6d83730ea1cd93c29b61e6568e2fc")

// 		if err != nil {
// 			c.Abort() //阻止执行
// 			httpx.SendError(c, errorx.NewCodeError(403, err.Error()))
// 			return
// 		}
// 		// fmt.Println(config.GetConfig())
// 		if config.GetConfig().Mode != "loc" {
// 			// mc.UserId
// 			userInfo, err := services.UserRpcSvc.GetUserInfo(int64(mc.UserId))

// 			if err != nil {
// 				httpx.SendError(c, errorx.NewCodeError(401, "request header Authorization formal error 2"))
// 				fmt.Println(err)
// 				c.Abort() //阻止执行
// 				return
// 			}

// 			c.Set("userId", int64(mc.UserId))
// 			c.Set("phone", userInfo.Data.Phone)
// 			c.Set("nickname", userInfo.Data.NickName)
// 		} else {
// 			c.Set("userId", int64(21))
// 			c.Set("phone", "6309610098888")
// 			c.Set("nickname", "Max")
// 		}
// 		c.Next()
// 	}
// }
