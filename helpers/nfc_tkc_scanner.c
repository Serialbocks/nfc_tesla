#include <furi/furi.h>
#include <nfc/nfc_poller.h>

#include "nfc_tkc_scanner.h"

struct NfcTkcScanner {
    Nfc* nfc;

    void* context;

    FuriThread* scan_worker;
};

NfcTkcScanner* nfc_tkc_scanner_alloc(Nfc* nfc) {
    UNUSED(nfc);
    return NULL;
}

void nfc_tkc_scanner_free(NfcTkcScanner* instance) {
    UNUSED(instance);
}

void nfc_tkc_scanner_start(NfcTkcScanner* instance, NfcTkcScannerCallback callback, void* context) {
    UNUSED(instance);
    UNUSED(callback);
    UNUSED(context);
}

void nfc_tkc_scanner_stop(NfcTkcScanner* instance) {
    UNUSED(instance);
}
