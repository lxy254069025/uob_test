package unigate

import "context"

type PaymentCardReaderRequest struct {
	PaymentCardReaderIdentifier string `json:"paymentCardReaderIdentifier"` // iphone SDK when a card is read
}

func (c *Client) AppleTapToPayReader(ctx context.Context, req PaymentCardReaderRequest) (*TransactionResponse, error) {
	var out TransactionResponse
	if err := c.doJSON(ctx, "POST", "/api/AppleTapToPayToken/PaymentCardReader", nil, req, &out); err != nil {
		return nil, err
	}
	return &out, nil
}
