package main

import (
	"flag"
	"uopenbox/martbox-api/config"
	"uopenbox/martbox-api/router"

	"github.com/gin-gonic/gin"
	swaggerFiles "github.com/swaggo/files"
	ginSwagger "github.com/swaggo/gin-swagger"
)

var (
	configFile = flag.String("f", "etc/home.yaml", "the config file")
)

func main() {
	flag.Parse()

	config.InitConfig(*configFile)

	r := gin.Default()

	router.InitRouter(r)

	//本地和测试模式的时候生成文档
	if config.GetConfig().Mode == "loc" || config.GetConfig().Mode == "test" {
		r.GET("/swagger/*any", ginSwagger.WrapHandler(swaggerFiles.Handler))
	}

	//上传
	// r.Use(middleware.CheckToken()).POST("/upload", v1.UploadFile)

	// if config.GetConfig().Mode != "loc" {
	// 	r.Static("/services", "/data/services")
	// }

	r.Run(config.GetConfig().Listen)
}
