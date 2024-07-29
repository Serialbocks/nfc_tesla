#pragma once

#include <nfc/nfc_listener.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_listener.h>
#include <nfc/helpers/iso14443_crc.h>

#include <mbedtls/3rdparty/p256-m/p256-m/p256-m.h>
#include <mbedtls/sha1.h>
#include <mbedtls/aes.h>

typedef enum {
    NfcTkcListenerEventTypeDetected,
    NfcTkcListenerEventTypeNotDetected,
    NfcTkcListenerEventTypeError
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
    uint8_t* public_key;
    uint8_t* private_key;

    NfcTkcListenerCallback callback;
    void* context;
} NfcTkcListener;

NfcTkcListener* nfc_tkc_listener_alloc(void* app);
void nfc_tkc_listener_free(NfcTkcListener* instance);
void nfc_tkc_listener_start(NfcTkcListener* instance, NfcTkcListenerCallback callback);
void nfc_tkc_listener_stop(NfcTkcListener* instance);
