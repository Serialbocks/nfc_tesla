#pragma once

#include <nfc/nfc_listener.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_listener.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_listener_i.h>

typedef enum {
    NfcTkcListenerEventTypeDetected,
    NfcTkcListenerEventTypeNotDetected,
} NfcTkcListenerEventType;

typedef struct {
    void* unused;
} NfcTkcListenerEventData;

typedef struct {
    NfcTkcListenerEventType type;
    NfcTkcListenerEventData data;
} NfcTkcListenerEvent;

typedef void (*NfcTkcListenerCallback)(NfcTkcListenerEvent event, void* context);

typedef struct NfcTkcListener {
    Nfc* nfc;
    NfcListener* listener;
    const Iso14443_4aData* iso14443_4adata;

    BitBuffer* tx_buffer;

    NfcTkcListenerCallback callback;
    void* context;
} NfcTkcListener;

NfcTkcListener* nfc_tkc_listener_alloc(void* app);
void nfc_tkc_listener_free(NfcTkcListener* instance);
void nfc_tkc_listener_start(NfcTkcListener* instance, NfcTkcListenerCallback callback);
void nfc_tkc_listener_stop(NfcTkcListener* instance);
