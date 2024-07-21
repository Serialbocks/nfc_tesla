#include <furi.h>

#include "nfc_tesla.h"

#include "nfc_tkc_listener.h"

NfcTkcListener* nfc_tkc_listener_alloc(void* appd) {
    NfcTkcListener* instance = malloc(sizeof(NfcTkcListener));
    NfcTeslaApp* app = appd;
    instance->context = app;
    instance->iso14443_4adata = nfc_device_get_data(app->nfc_device, NfcProtocolIso14443_4a);
    instance->nfc = app->nfc;
    instance->listener =
        nfc_listener_alloc(instance->nfc, NfcProtocolIso14443_4a, instance->iso14443_4adata);
    return instance;
}
void nfc_tkc_listener_free(NfcTkcListener* instance) {
    furi_assert(instance);
    nfc_listener_free(instance->listener);
    free(instance);
}

static NfcCommand tkc_listener_start_callback(NfcGenericEvent event, void* contextd) {
    NfcTkcListener* instance = contextd;
    NfcTkcListenerEvent result = {.type = NfcTkcListenerEventTypeNotDetected};
    UNUSED(event);

    instance->callback(result, instance->context);
    return NfcCommandContinue;
}

void nfc_tkc_listener_start(NfcTkcListener* instance, NfcTkcListenerCallback callback) {
    furi_assert(instance);
    furi_assert(callback);

    instance->callback = callback;
    nfc_listener_start(instance->listener, tkc_listener_start_callback, instance);
}

void nfc_tkc_listener_stop(NfcTkcListener* instance) {
    furi_assert(instance);

    nfc_listener_stop(instance->listener);
}
