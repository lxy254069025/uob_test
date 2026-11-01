package unigate

import "time"

const defaultTimeout = 30 * time.Second

// Operation mirrors unigate_operation in unigate.h.
type Operation int

const (
	OperationEMV Operation = iota + 1
	OperationToken
	OperationCardSwipe
	OperationKeypad
	OperationManual
	OperationInAppApplePay
	OperationTECApplePay
	OperationGooglePay
	OperationIncrementalAuthorize
	OperationTipAdjust
	OperationCapture
	OperationRefund
	OperationVoid
	OperationGetByMagTranID
	OperationGetByCustomerTranID
	OperationAppleTapToPayReader
)

// ClientOptions mirrors unigate_client_options in unigate.h.
type ClientOptions struct {
	BaseURL        string
	CustomerCode   string
	UserName       string
	Password       string
	CAPath         string
	Timeout        time.Duration
	LogRequestBody bool
}

// KeyValue mirrors unigate_key_value in unigate.h.
type KeyValue struct {
	Key   string
	Value string
}

// Request mirrors unigate_request in unigate.h.
type Request struct {
	Operation                Operation
	CustomerTransactionID    string
	TransactionType          string
	Amount                   float64
	DataType                 string
	Data                     string
	PaymentType              string
	Token                    string
	Track1                   string
	Track2                   string
	Track3                   string
	MagnePrint               string
	MagnePrintStatus         string
	KSN                      string
	KeyVariant               string
	CVV                      string
	Zip                      string
	IsFallback               string
	AddressLine1             string
	AddressLine2             string
	City                     string
	State                    string
	Country                  string
	NameOnCard               string
	PAN                      string
	ExpirationDate           string
	ApplePayToken            string
	GooglePayToken           string
	ReferenceMagTranID       string
	TipAmount                float64
	HasAmount                bool
	TransactionInputDetails  []KeyValue
	MagTranID                string
	CustomerTransactionQuery string
	ReaderIdentifier         string
}

// Response mirrors the public fields of unigate_response in unigate.h.
type Response struct {
	StatusCode   int
	Method       string
	Path         string
	Body         []byte
	FaultCode    string
	FaultMessage string
	FaultTraceID string
}
