//go:build !cgo || (!darwin && !linux) || (!amd64 && !arm64)

package unigate

import "errors"

var errCgoUnavailable = errors.New(
	"unigate: native C SDK requires CGO_ENABLED=1 on darwin or linux amd64/arm64",
)

// Execute returns an error when the native C SDK is unavailable.
func Execute(ClientOptions, Request) (*Response, error) {
	return nil, errCgoUnavailable
}

// RedactJSON avoids returning raw input when the native C SDK is unavailable.
func RedactJSON([]byte) string {
	return "<redact-unavailable>"
}
