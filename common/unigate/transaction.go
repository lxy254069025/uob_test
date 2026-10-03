package unigate

import "context"

// EncryptedData
type EncryptedData struct {
	DataType string `json:"dataType"`
	Data     string `json:"data"`
}

type EMVDataInput struct {
	EncryptedData EncryptedData `json:"encryptedData"`
	PaymentType   string        `json:"paymentType"`
}

type EMVRequest struct {
	CustomerTransactionID string           `json:"customerTransactionID"`
	TransactionInput      TransactionInput `json:"transactionInput"`
	DataInput             EMVDataInput     `json:"dataInput"`
}

func (c *Client) EMVTransaction(ctx context.Context, req EMVRequest) (*TransactionResponse, error) {
	var out TransactionResponse
	if err := c.doJSON(ctx, "POST", "/api/Transaction/EMV", nil, req, &out); err != nil {
		return nil, err
	}

	return &out, nil
}

func (c *Client) EMVSale(ctx context.Context, customerTransactionID string, amount float64, arqc string) (*TransactionResponse, error) {
	return c.EMVTransaction(ctx, EMVRequest{
		CustomerTransactionID: customerTransactionID,
		TransactionInput: TransactionInput{
			TransactionType: TxnTypeSale,
			Amount:          amount,
		},
		DataInput: EMVDataInput{
			EncryptedData: EncryptedData{
				DataType: "ARQC",
				Data:     arqc,
			},
			PaymentType: PaymentTypeCredit,
		},
	})
}

func (c *Client) EMVAuthorize(ctx context.Context, customerTransactionID string, amount float64, arqc string) (*TransactionResponse, error) {
	return c.EMVTransaction(ctx, EMVRequest{
		CustomerTransactionID: customerTransactionID,
		TransactionInput: TransactionInput{
			TransactionType: TxnTypeAuthorize,
			Amount:          amount,
		},
		DataInput: EMVDataInput{
			EncryptedData: EncryptedData{
				DataType: "ARQC",
				Data:     arqc,
			},
			PaymentType: PaymentTypeCredit,
		},
	})
}

type TokenRequest struct {
	CustomerTransactionID string           `json:"customerTransactionID"`
	TransactionInput      TransactionInput `json:"transactionInput"`
	Token                 string           `json:"token"`
}

// Token交易， 上一次交易过，这次直接消费
func (c *Client) TokenTransaction(ctx context.Context, req TokenRequest) (*TransactionResponse, error) {
	var out TransactionResponse
	if err := c.doJSON(ctx, "POST", "/api/Transaction/Token", nil, req, &out); err != nil {
		return nil, err
	}

	return &out, nil
}

type EncryptedCardSwipe struct {
	Track1           string `json:"track1"`
	Track2           string `json:"track2"`
	Track3           string `json:"track3"`
	MagnePrint       string `json:"magnePrint"`
	MagnePrintStatus string `json:"magnePrintStatus"`
	KSN              string `json:"ksn"`
	KeyVariant       string `json:"keyVariant"`
}

type CardSwipeInput struct {
	EncryptedCardSwipe EncryptedCardSwipe `json:"encryptedCardSwipe"`
	CVV                string             `json:"cvv"`
	Zip                string             `json:"zip"`
	PaymentType        string             `json:"paymentType"`
	IsFallBack         string             `json:"isFallBack"` // "true" / "false"
}

type CardSwipeRequest struct {
	CustomerTransactionID string           `json:"customerTransactionID"`
	TransactionInput      TransactionInput `json:"transactionInput"`
	CardSwipeInput        CardSwipeInput   `json:"cardSwipeInput"`
}

// 磁条刷卡交易
func (c *Client) CardSwipeTransaction(ctx context.Context, req CardSwipeRequest) (*TransactionResponse, error) {
	var out TransactionResponse
	if err := c.doJSON(ctx, "POST", "/api/Transaction/CardSwipe", nil, req, &out); err != nil {
		return nil, err
	}
	return &out, nil
}

type EncryptedKeyPadEntry struct {
	Track1     string `json:"track1"`
	Track2     string `json:"track2"`
	Track3     string `json:"track3"`
	KSN        string `json:"ksn"`
	KeyVariant string `json:"keyVariant"` // e.g. "DukptAes256DataVariant"
}

type KeyPadEntryInput struct {
	EncryptedKeyPadEntry EncryptedKeyPadEntry `json:"encryptedKeyPadEntry"`
	CVV                  string               `json:"cvv"`
	Zip                  string               `json:"zip"`
	PaymentType          string               `json:"paymentType"`
}

type KeyPadEntryRequest struct {
	CustomerTransactionID string           `json:"customerTransactionID"`
	TransactionInput      TransactionInput `json:"transactionInput"`
	KeyPadEntryInput      KeyPadEntryInput `json:"keyPadEntryInput"`
}

func (c *Client) KeyPadEntryTransaction(ctx context.Context, req KeyPadEntryRequest) (*TransactionResponse, error) {
	var out TransactionResponse
	if err := c.doJSON(ctx, "POST", "/api/Transaction/KeyPadEntry", nil, req, &out); err != nil {
		return nil, err
	}
	return &out, nil
}

type ManualEntryInput struct {
	AddressLine1   string `json:"addressLine1"`
	AddressLine2   string `json:"addressLine2"`
	City           string `json:"city"`
	State          string `json:"state"`
	Country        string `json:"country"`
	Zip            string `json:"zip"`
	NameOnCard     string `json:"nameOnCard"`
	PAN            string `json:"pan"`
	ExpirationDate string `json:"expirationDate"`
	CVV            string `json:"cvv"`
}

type ManualEntryRequest struct {
	CustomerTransactionID string           `json:"customerTransactionID"`
	TransactionInput      TransactionInput `json:"transactionInput"`
	ManualEntryInput      ManualEntryInput `json:"manualEntryInput"`
}

// 卡不在场
func (c *Client) ManualEntryTransaction(ctx context.Context, req ManualEntryRequest) (*TransactionResponse, error) {
	var out TransactionResponse
	if err := c.doJSON(ctx, "POST", "/api/Transaction/ManualEntry", nil, req, &out); err != nil {
		return nil, err
	}
	return &out, nil
}

type ApplePayRequest struct {
	CustomerTransactionID string           `json:"customerTransactionID"`
	TransactionInput      TransactionInput `json:"transactionInput"`
	ApplePayToken         string           `json:"applePayToken"`
}

func (c *Client) InAppApplePayTransaction(ctx context.Context, req ApplePayRequest) (*TransactionResponse, error) {
	var out TransactionResponse
	if err := c.doJSON(ctx, "POST", "/api/Transaction/InAppApplePay", nil, req, &out); err != nil {
		return nil, err
	}
	return &out, nil
}

func (c *Client) TECApplePayTransaction(ctx context.Context, req ApplePayRequest) (*TransactionResponse, error) {
	var out TransactionResponse
	if err := c.doJSON(ctx, "POST", "/api/Transaction/TECApplePay", nil, req, &out); err != nil {
		return nil, err
	}
	return &out, nil
}

type GooglePayRequest struct {
	CustomerTransactionID string           `json:"customerTransactionID"`
	TransactionInput      TransactionInput `json:"transactionInput"`
	GooglePayToken        string           `json:"googlePayToken"`
}

func (c *Client) GooglePayTransaction(ctx context.Context, req GooglePayRequest) (*TransactionResponse, error) {
	var out TransactionResponse
	if err := c.doJSON(ctx, "POST", "/api/Transaction/GooglePay", nil, req, &out); err != nil {
		return nil, err
	}
	return &out, nil
}
