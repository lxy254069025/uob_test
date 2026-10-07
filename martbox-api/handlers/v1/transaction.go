package v1

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"errors"
	"fmt"
	"log"
	"net/http"
	"strings"

	"uopenbox/common/unigate"
	"uopenbox/martbox-api/config"

	"github.com/gin-gonic/gin"
)

type EmvRequest struct {
	// Data 是卡片数据，具体格式由接口方法决定：
	// /v1/transaction/emv 要求 ARQC（读卡器产出的裸 EMV 数据）。
	Data string `json:"data" binding:"required"`
	// PaymentType 为空时按 Credit 处理。
	PaymentType string `json:"paymentType"`
}

// emvAuthorizeAmount 是 EMV 授权方法自己使用的金额。
// TODO: 接入订单/机器信息后，金额应该由订单带过来，而不是写死。
const emvAuthorizeAmount = 100.00

func Emv(g *gin.Context) {
	var req EmvRequest

	if err := g.ShouldBindJSON(&req); err != nil {
		g.JSON(http.StatusBadRequest, gin.H{"error": "参数错误: " + err.Error()})
		return
	}

	data := strings.TrimSpace(req.Data)
	if data == "" {
		// 空数据送给 Magensa 只会换回一个语焉不详的 UnknownError，在入口就拦掉。
		g.JSON(http.StatusBadRequest, gin.H{"error": "data 不能为空"})
		return
	}

	customerTransactionID := gorderId() //order id
	ctx := context.Background()

	client := newUnigateClient()

	// 授权交易：报文格式由 emvAuthorizeRequest 自己组装，入参只有 paymentType 和 data。
	resp, err := client.EMVTransaction(ctx,
		emvAuthorizeRequest(customerTransactionID, req.PaymentType, data))
	if err != nil {
		respondUnigateError(g, err)
		return
	}

	// Magensa 判定重放时会直接返回上一次的授权结果，这次并没有真的发起新交易。
	// 设备若照常出货，就会出现"出两次货、只扣一次钱"。
	if notice, ok := replayNotice(resp); ok {
		log.Printf("emv: 检测到重放交易，未发起新授权 %s", notice)
	}

	g.JSON(http.StatusOK, resp)
}

// replayNotice 在 Magensa 判定为重放时返回告警描述，否则返回 false。
func replayNotice(resp *unigate.TransactionResponse) (string, bool) {
	if resp == nil || !resp.DataOutput.IsReplay {
		return "", false
	}

	return fmt.Sprintf("customerTransactionID=%s magTranID=%s panLast4=%s 授权=%t",
		resp.CustomerTransactionID, resp.MagTranID, resp.DataOutput.PanLast4,
		resp.TransactionOutput.IsTransactionApproved), true
}

// emvAuthorizeRequest 组装 /api/Transaction/EMV 的授权报文。
//
// 这是"EMV 读卡授权"这个方法自己的提交格式：dataType 固定为 ARQC，
// 交易类型固定为 AUTHORIZE，金额取方法自己的默认值。
// 换一种支付方式（例如 iPhone Tap to Pay）应该是另一个方法，
// 由那个方法组装成它自己的格式。
func emvAuthorizeRequest(customerTransactionID, paymentType, data string) unigate.EMVRequest {
	paymentType = strings.TrimSpace(paymentType)
	if paymentType == "" {
		paymentType = unigate.PaymentTypeCredit
	}

	return unigate.EMVRequest{
		CustomerTransactionID: customerTransactionID,
		TransactionInput: unigate.TransactionInput{
			TransactionType: unigate.TxnTypeAuthorize,
			Amount:          emvAuthorizeAmount,
		},
		DataInput: unigate.EMVDataInput{
			EncryptedData: unigate.EncryptedData{
				DataType: unigate.DataTypeARQC,
				Data:     data,
			},
			PaymentType: paymentType,
		},
	}
}

func gorderId() string {
	b := make([]byte, 16)
	_, err := rand.Read(b)
	if err != nil {
		panic(err)
	}

	return hex.EncodeToString(b)
}

type CaptureRequest struct {
	ReferenceMagTran string `json:"reference_mag_tran"`
}

func Capture(g *gin.Context) {
	var req CaptureRequest
	if err := g.ShouldBindJSON(&req); err != nil {
		g.JSON(http.StatusBadRequest, gin.H{"error": "参数错误: " + err.Error()})
		return
	}

	magTranID := strings.TrimSpace(req.ReferenceMagTran)
	if magTranID == "" {
		g.JSON(http.StatusBadRequest, gin.H{"error": "reference_mag_tran 不能为空"})
		return
	}

	client := newUnigateClient()

	ctx := context.Background()

	//如果金额不足，追加授权。
	// resp, err := client.IncrementalAuthorize(ctx, unigate.ReferenceAmountRequest{
	// 	ReferenceMagTranID: magTranID, // 引用交易ID
	// 	Amount:             100.00,
	// })
	// if err != nil {
	// 	respondUnigateError(g, err)
	// 	return
	// }

	//扣款
	resp, err := client.Capture(ctx, unigate.ReferenceAmountRequest{
		ReferenceMagTranID: magTranID,
		Amount:             20.00,
	})

	if err != nil {
		respondUnigateError(g, err)
		return
	}

	g.JSON(http.StatusOK, resp)
}

// newUnigateClient 按配置构造 Magensa 客户端。
//
// TODO: 凭据仍然硬编码在源码里（且已经提交进 git），应该挪到配置并轮换掉。
func newUnigateClient() *unigate.Client {
	client := unigate.NewClient("", "EF09625335", "MAG888811829", "86keSz-CrA2EDMegaD_4")
	client.LogRequestBody = config.GetConfig().Unigate.LogRequestBody

	return client
}

// respondUnigateError 把上游错误转成给设备的响应。
//
// 这里绝不能用 log.Fatal：一次上游校验失败不该把整个 API 进程杀掉，
// 那会同时断掉所有设备的长连接。
func respondUnigateError(g *gin.Context, err error) {
	var apiErr *unigate.APIError
	if !errors.As(err, &apiErr) {
		log.Printf("unigate 调用失败: %v", err)
		g.JSON(http.StatusBadGateway, gin.H{"error": "upstream_error", "message": err.Error()})
		return
	}

	log.Printf("unigate 调用失败: %s %s status=%d code=%s traceID=%s message=%s",
		apiErr.Method, apiErr.Path, apiErr.StatusCode, apiErr.Code, apiErr.TraceID, apiErr.Message)

	code := apiErr.Code
	if code == "" {
		code = "upstream_error"
	}

	// Magensa 的 4xx 说明送上去的数据有问题，对设备来说就是请求错误；5xx 才算上游故障。
	status := http.StatusBadGateway
	if apiErr.StatusCode >= 400 && apiErr.StatusCode < 500 {
		status = http.StatusBadRequest
	}

	g.JSON(status, gin.H{
		"error":   code,
		"message": apiErr.Message,
		"traceID": apiErr.TraceID, // 设备端报障时可以直接把它给 Magensa
	})
}
