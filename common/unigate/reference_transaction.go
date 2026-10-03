package unigate

import (
	"context"
	"fmt"
	"net/url"
)

type ReferenceAmountRequest struct {
	ReferenceMagTranID string  `json:"referenceMagTranID"`
	Amount             float64 `json:"amount"`
}

func (c *Client) IncrementalAuthorize(ctx context.Context, req ReferenceAmountRequest) (*TransactionResponse, error) {
	var out TransactionResponse
	if err := c.doJSON(ctx, "POST", "/api/ReferenceTransaction/AUTHORIZE", nil, req, &out); err != nil {
		return nil, err
	}
	return &out, nil
}

type TipAdjustRequest struct {
	ReferenceMagTranID      string     `json:"referenceMagTranID"`
	TipAmount               float64    `json:"tipAmount"`
	TransactionInputDetails []KeyValue `json:"transactionInputDetails,omitempty"`
}

func (c *Client) TipAdjust(ctx context.Context, req TipAdjustRequest) (*TransactionResponse, error) {
	var out TransactionResponse
	if err := c.doJSON(ctx, "POST", "/api/ReferenceTransaction/TIPADJUST", nil, req, &out); err != nil {
		return nil, err
	}
	return &out, nil
}

func (c *Client) Capture(ctx context.Context, req ReferenceAmountRequest) (*TransactionResponse, error) {
	var out TransactionResponse
	if err := c.doJSON(ctx, "POST", "/api/ReferenceTransaction/CAPTURE", nil, req, &out); err != nil {
		return nil, err
	}
	return &out, nil
}

func (c *Client) Refund(ctx context.Context, req ReferenceAmountRequest) (*TransactionResponse, error) {
	var out TransactionResponse
	if err := c.doJSON(ctx, "POST", "/api/ReferenceTransaction/REFUND", nil, req, &out); err != nil {
		return nil, err
	}
	return &out, nil
}

type VoidRequest struct {
	ReferenceMagTranID string   `json:"referenceMagTranID"`
	Amount             *float64 `json:"amount,omitempty"`
}

func (c *Client) Void(ctx context.Context, req VoidRequest) (*TransactionResponse, error) {
	var out TransactionResponse
	if err := c.doJSON(ctx, "POST", "/api/ReferenceTransaction/VOID", nil, req, &out); err != nil {
		return nil, err
	}
	return &out, nil
}

func (c *Client) FullVoid(ctx context.Context, referenceMagTranID string) (*TransactionResponse, error) {
	return c.Void(ctx, VoidRequest{ReferenceMagTranID: referenceMagTranID})
}

// PartialVoid is a convenience wrapper for Void with a specific amount.
func (c *Client) PartialVoid(ctx context.Context, referenceMagTranID string, amount float64) (*TransactionResponse, error) {
	return c.Void(ctx, VoidRequest{ReferenceMagTranID: referenceMagTranID, Amount: &amount})
}

func (c *Client) GetTransactionByMagTranID(ctx context.Context, magTranID string) (*TransactionResponse, error) {
	var out TransactionResponse
	path := fmt.Sprintf("/api/Transaction/%s", url.PathEscape(magTranID))
	if err := c.doJSON(ctx, "GET", path, nil, nil, &out); err != nil {
		return nil, err
	}
	return &out, nil
}

func (c *Client) GetTransactionsByCustomerTransactionID(ctx context.Context, custTranID string) (*TransactionResponse, error) {
	var out TransactionResponse
	q := url.Values{"custTranID": {custTranID}}
	if err := c.doJSON(ctx, "GET", "/api/Transaction", q, nil, &out); err != nil {
		return nil, err
	}
	return &out, nil
}
