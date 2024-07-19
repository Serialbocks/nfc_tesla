#pragma once

#include <nfc/nfc.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller_i.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_listener.h>
#include "protocol/tkc.h"

typedef enum {
    NfcTkcScannerEventTypeDetected,
    NfcTkcScannerEventTypeNotDetected,
} NfcTkcScannerEventType;

typedef struct {
    Tkc* tkc_data;
} NfcTkcScannerEventData;

typedef struct {
    NfcTkcScannerEventType type;
    NfcTkcScannerEventData data;
} NfcTkcScannerEvent;

typedef void (*NfcTkcScannerCallback)(NfcTkcScannerEvent event, void* context);

typedef struct NfcTkcScanner NfcTkcScanner;

NfcTkcScanner* nfc_tkc_scanner_alloc(Nfc* nfc);
void nfc_tkc_scanner_free(NfcTkcScanner* instance);

void nfc_tkc_scanner_start(NfcTkcScanner* instance, NfcTkcScannerCallback callback, void* context);

void nfc_tkc_scanner_stop(NfcTkcScanner* instance);