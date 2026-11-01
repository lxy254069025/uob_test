//go:build !cgo

package unigate

import (
	"errors"
	"testing"
)

func TestNativeSDKIsUnavailableWithoutCgo(t *testing.T) {
	if response, err := Execute(ClientOptions{}, Request{}); response != nil ||
		!errors.Is(err, errCgoUnavailable) {
		t.Fatalf("Execute() = (%v, %v), want (nil, %v)", response, err, errCgoUnavailable)
	}
	if got := RedactJSON([]byte(`{"track2":"secret"}`)); got != "<redact-unavailable>" {
		t.Fatalf("RedactJSON() = %q, want unavailable marker", got)
	}
}
