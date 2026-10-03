package unigate

const (
	TxnTypeSale      = "SALE"      //直接消费
	TxnTypeAuthorize = "AUTHORIZE" //预授权交易
)

const (
	PaymentTypeCredit   = "Credit"
	PaymentTypeImplicit = "Implicit"
)

type KeyValue struct {
	Key   string `json:"key"`
	Value string `json:"value"`
}

type TransactionInput struct {
	TransactionType string  `json:"transactionType"`
	Amount          float64 `json:"amount"`
}

type TransactionOutput struct {
	TransactionID               string                 `json:"transactionID"`
	IsTransactionApproved       bool                   `json:"isTransactionApproved"`
	TransactionStatus           string                 `json:"transactionStatus"`
	TransactionMessage          string                 `json:"transactionMessage"`
	AuthCode                    string                 `json:"authCode"`
	AuthorizedAmount            float64                `json:"authorizedAmount"`
	ProcessorConvertedResponse  map[string]interface{} `json:"processorConvertedResponse"`
	ProcessorNormalizedResponse map[string]interface{} `json:"processorNormalizedResponse"`
	AVSResult                   string                 `json:"avsResult"`
	CVVResult                   *string                `json:"cvvResult"`
	IssuerAuthenticationData    string                 `json:"issuerAuthenticationData"`
	IssuerScriptTemplate1       string                 `json:"issuerScriptTemplate1"`
	IssuerScriptTemplate2       string                 `json:"issuerScriptTemplate2"`
	Token                       string                 `json:"token"`
	TransactionOutputDetails    []KeyValue             `json:"transactionOutputDetails"`
}

type TransactionResponse struct {
	DataOutput              DataOutput        `json:"dataOutput"`
	TraceID                 string            `json:"traceID"`
	MagTranID               string            `json:"magTranID"`
	CustomerTransactionID   string            `json:"customerTransactionID"`
	TransactionUTCTimeStamp string            `json:"transactionUTCTimeStamp"`
	TransactionOutput       TransactionOutput `json:"transactionOutput"`
	AdditionalResponseData  interface{}       `json:"additionalResponseData"`
	Error                   string            `json:"error"`
}

type DataOutput struct {
	CardID               string     `json:"cardId"`
	PanLast4             string     `json:"panLast4"`
	IsReplay             bool       `json:"isReplay"`
	AdditionalOutputData []KeyValue `json:"additionalOutputData"`
}

func (o TransactionOutput) NormalizedField(key string) string {
	if o.ProcessorNormalizedResponse == nil {
		return ""
	}
	if v, ok := o.ProcessorNormalizedResponse[key].(string); ok {
		return v
	}
	return ""
}
