//go:build cgo && (darwin || linux) && (amd64 || arm64)

package unigate

/*
#cgo CFLAGS: -I${SRCDIR}
#cgo darwin,amd64 LDFLAGS: ${SRCDIR}/lib/darwin_amd64/libunigate.a
#cgo darwin,arm64 LDFLAGS: ${SRCDIR}/lib/darwin_arm64/libunigate.a
#cgo linux,amd64 LDFLAGS: ${SRCDIR}/lib/linux_amd64/libunigate.a -pthread
#cgo linux,arm64 LDFLAGS: ${SRCDIR}/lib/linux_arm64/libunigate.a -pthread

#define UNIGATE_C_SDK_REVISION 2026110101

#include <stdlib.h>
#include "unigate.h"

extern void unigateGoLog(char *message, void *user_data);
*/
import "C"

import (
	"fmt"
	"log"
	"unsafe"
)

type cStringPool struct {
	values []*C.char
}

func (p *cStringPool) add(value string) *C.char {
	valueC := C.CString(value)
	p.values = append(p.values, valueC)
	return valueC
}

func (p *cStringPool) free() {
	for _, value := range p.values {
		C.free(unsafe.Pointer(value))
	}
}

//export unigateGoLog
func unigateGoLog(message *C.char, userData unsafe.Pointer) {
	log.Printf("%s", C.GoString(message))
}

// Execute calls unigate_execute_request. The C SDK builds the request URL and
// JSON body, performs HTTP/TLS, and parses Magensa fault fields.
func Execute(options ClientOptions, request Request) (*Response, error) {
	timeout := options.Timeout
	if timeout <= 0 {
		timeout = defaultTimeout
	}

	var pool cStringPool
	defer pool.free()
	cOptions := C.unigate_client_options{
		base_url:      pool.add(options.BaseURL),
		customer_code: pool.add(options.CustomerCode),
		user_name:     pool.add(options.UserName),
		password:      pool.add(options.Password),
		ca_path:       pool.add(options.CAPath),
		timeout_ms:    C.long(timeout.Milliseconds()),
	}
	if options.LogRequestBody {
		cOptions.log_request_body = 1
		cOptions.log_callback = C.unigate_log_callback(C.unigateGoLog)
	}

	cRequest := C.unigate_request{
		operation:                      C.unigate_operation(request.Operation),
		customer_transaction_id:        pool.add(request.CustomerTransactionID),
		transaction_type:               pool.add(request.TransactionType),
		amount:                         C.double(request.Amount),
		data_type:                      pool.add(request.DataType),
		data:                           pool.add(request.Data),
		payment_type:                   pool.add(request.PaymentType),
		token:                          pool.add(request.Token),
		track1:                         pool.add(request.Track1),
		track2:                         pool.add(request.Track2),
		track3:                         pool.add(request.Track3),
		magne_print:                    pool.add(request.MagnePrint),
		magne_print_status:             pool.add(request.MagnePrintStatus),
		ksn:                            pool.add(request.KSN),
		key_variant:                    pool.add(request.KeyVariant),
		cvv:                            pool.add(request.CVV),
		zip:                            pool.add(request.Zip),
		is_fallback:                    pool.add(request.IsFallback),
		address_line1:                  pool.add(request.AddressLine1),
		address_line2:                  pool.add(request.AddressLine2),
		city:                           pool.add(request.City),
		state:                          pool.add(request.State),
		country:                        pool.add(request.Country),
		name_on_card:                   pool.add(request.NameOnCard),
		pan:                            pool.add(request.PAN),
		expiration_date:                pool.add(request.ExpirationDate),
		apple_pay_token:                pool.add(request.ApplePayToken),
		google_pay_token:               pool.add(request.GooglePayToken),
		reference_mag_tran_id:          pool.add(request.ReferenceMagTranID),
		tip_amount:                     C.double(request.TipAmount),
		mag_tran_id:                    pool.add(request.MagTranID),
		customer_transaction_id_query:  pool.add(request.CustomerTransactionQuery),
		payment_card_reader_identifier: pool.add(request.ReaderIdentifier),
	}
	if request.HasAmount {
		cRequest.has_amount = 1
	}

	var details unsafe.Pointer
	var detailStrings cStringPool
	defer detailStrings.free()
	if len(request.TransactionInputDetails) > 0 {
		size := C.size_t(len(request.TransactionInputDetails)) * C.size_t(unsafe.Sizeof(C.unigate_key_value{}))
		details = C.calloc(1, size)
		if details == nil {
			return nil, fmt.Errorf("allocating transaction input details")
		}
		defer C.free(details)
		detailValues := unsafe.Slice((*C.unigate_key_value)(details), len(request.TransactionInputDetails))
		for i, detail := range request.TransactionInputDetails {
			detailValues[i].key = detailStrings.add(detail.Key)
			detailValues[i].value = detailStrings.add(detail.Value)
		}
		cRequest.transaction_input_details = (*C.unigate_key_value)(details)
		cRequest.transaction_input_detail_count = C.size_t(len(request.TransactionInputDetails))
	}

	var nativeResponse C.unigate_response
	rc := C.unigate_execute_request(&cOptions, &cRequest, &nativeResponse)
	defer C.unigate_response_free(&nativeResponse)
	if rc != 0 {
		if nativeResponse.error != nil {
			return nil, fmt.Errorf("%s", C.GoString(nativeResponse.error))
		}
		return nil, fmt.Errorf("native request failed")
	}

	response := &Response{
		StatusCode: int(nativeResponse.status_code),
	}
	if nativeResponse.method != nil {
		response.Method = C.GoString(nativeResponse.method)
	}
	if nativeResponse.path != nil {
		response.Path = C.GoString(nativeResponse.path)
	}
	if nativeResponse.body != nil {
		response.Body = C.GoBytes(unsafe.Pointer(nativeResponse.body), C.int(nativeResponse.body_len))
	}
	if nativeResponse.fault_code != nil {
		response.FaultCode = C.GoString(nativeResponse.fault_code)
	}
	if nativeResponse.fault_message != nil {
		response.FaultMessage = C.GoString(nativeResponse.fault_message)
	}
	if nativeResponse.fault_trace_id != nil {
		response.FaultTraceID = C.GoString(nativeResponse.fault_trace_id)
	}
	return response, nil
}

// RedactJSON calls the C SDK redaction implementation.
func RedactJSON(body []byte) string {
	if len(body) == 0 {
		return "<not-json>"
	}
	cBody := C.CString(string(body))
	defer C.free(unsafe.Pointer(cBody))

	result := C.unigate_redact_json(cBody, C.size_t(len(body)))
	if result == nil {
		return "<redact-failed>"
	}
	defer C.unigate_free(unsafe.Pointer(result))
	return C.GoString(result)
}
