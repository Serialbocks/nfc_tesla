#include <furi.h>

#include "nfc_tesla.h"

#include "tkc_listener.h"

TkcListener* tkc_listener_alloc(void* appd) {
    TkcListener* instance = malloc(sizeof(TkcListener));
    NfcTeslaApp* app = appd;
    instance->context = app;
    instance->iso14443_4adata = nfc_device_get_data(app->nfc_device, NfcProtocolIso14443_4a);
    instance->nfc = app->nfc;
    instance->listener =
        nfc_listener_alloc(instance->nfc, NfcProtocolIso14443_4a, instance->iso14443_4adata);
    return instance;
}
void tkc_listener_free(TkcListener* instance) {
    furi_assert(instance);
    nfc_listener_free(instance->listener);
    free(instance);
}

static NfcCommand tkc_listener_start_callback(NfcGenericEvent event, void* contextd) {
    TkcListener* instance = contextd;
    TkcListenerEvent result = {.type = TkcListenerEventTypeNotDetected};
    UNUSED(event);

    instance->callback(result, instance->context);
    return NfcCommandContinue;
}

void tkc_listener_start(TkcListener* instance, TkcListenerCallback callback) {
    furi_assert(instance);
    furi_assert(callback);

    instance->callback = callback;
    nfc_listener_start(instance->listener, tkc_listener_start_callback, instance);
}

void tkc_listener_stop(TkcListener* instance) {
    furi_assert(instance);

    nfc_listener_stop(instance->listener);
}
