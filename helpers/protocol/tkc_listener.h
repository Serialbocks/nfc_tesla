#pragma once

#include <nfc/nfc_listener.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_listener.h>

typedef enum {
    TkcListenerEventTypeDetected,
    TkcListenerEventTypeNotDetected,
} TkcListenerEventType;

typedef struct {
    void* unused;
} TkcListenerEventData;

typedef struct {
    TkcListenerEventType type;
    TkcListenerEventData data;
} TkcListenerEvent;

typedef void (*TkcListenerCallback)(TkcListenerEvent event, void* context);

typedef struct TkcListener {
    Nfc* nfc;
    NfcListener* listener;
    const Iso14443_4aData* iso14443_4adata;

    TkcListenerCallback callback;
    void* context;
} TkcListener;

TkcListener* tkc_listener_alloc(void* app);
void tkc_listener_free(TkcListener* instance);
void tkc_listener_start(TkcListener* instance, TkcListenerCallback callback);
void tkc_listener_stop(TkcListener* instance);
