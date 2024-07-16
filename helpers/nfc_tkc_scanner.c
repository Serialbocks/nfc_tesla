#include <furi/furi.h>
#include <furi_hal.h>
#include <nfc/nfc_poller.h>

#include "protocol/tkc.h"
#include "protocol/tkc_poller.h"
#include "nfc_tkc_scanner.h"

#define TAG "NFC_TESLA_tkc_scanner"

typedef enum {
    NfcTkcScannerSessionStateIdle,
    NfcTkcScannerSessionStateActive,
    NfcTkcScannerSessionStateStopRequest,
} NfcTkcScannerSessionState;

struct NfcTkcScanner {
    Nfc* nfc;
    NfcTkcScannerSessionState session_state;

    Tkc* tkc_data;

    NfcTkcScannerCallback callback;
    void* context;

    FuriThread* scan_worker;
};

NfcTkcScanner* nfc_tkc_scanner_alloc(Nfc* nfc) {
    furi_assert(nfc);

    NfcTkcScanner* instance = malloc(sizeof(NfcTkcScanner));
    instance->nfc = nfc;
    instance->tkc_data = tkc_alloc();

    return instance;
}

void nfc_tkc_scanner_free(NfcTkcScanner* instance) {
    furi_assert(instance);

    tkc_free(instance->tkc_data);
    free(instance);
}

static int32_t nfc_tkc_scanner_worker(void* context) {
    furi_assert(context);
    NfcTkcScanner* instance = context;
    furi_assert(instance->session_state == NfcTkcScannerSessionStateActive);

    while(instance->session_state == NfcTkcScannerSessionStateActive) {
        tkc_reset(instance->tkc_data);
        Tkc tkc_data;
        TkcPollerError error = tkc_poller_detect(instance->nfc, &tkc_data);
        if(error != TkcPollerErrorNone) {
            FURI_LOG_D(TAG, "TkcPollerError %u", error);
            continue;
        }
        tkc_copy(instance->tkc_data, &tkc_data);
        NfcTkcScannerEvent event = {
            .type = NfcTkcScannerEventTypeDetected, .data = {.tkc_data = tkc_data}};
        instance->callback(event, instance->context);
        break;
    }

    instance->session_state = NfcTkcScannerSessionStateIdle;
    return 0;
}

void nfc_tkc_scanner_start(NfcTkcScanner* instance, NfcTkcScannerCallback callback, void* context) {
    furi_assert(instance);
    furi_assert(callback);

    instance->callback = callback;
    instance->context = context;

    instance->scan_worker = furi_thread_alloc();
    furi_thread_set_name(instance->scan_worker, "NfcTkcScanWorker");
    furi_thread_set_context(instance->scan_worker, instance);
    furi_thread_set_stack_size(instance->scan_worker, 4 * 1024);
    furi_thread_set_callback(instance->scan_worker, nfc_tkc_scanner_worker);
    furi_thread_start(instance->scan_worker);

    instance->session_state = NfcTkcScannerSessionStateActive;
}

void nfc_tkc_scanner_stop(NfcTkcScanner* instance) {
    furi_assert(instance);

    instance->session_state = NfcTkcScannerSessionStateStopRequest;
    furi_thread_join(instance->scan_worker);
    instance->session_state = NfcTkcScannerSessionStateIdle;

    furi_thread_free(instance->scan_worker);
    instance->scan_worker = NULL;
    instance->callback = NULL;
    instance->context = NULL;
}
