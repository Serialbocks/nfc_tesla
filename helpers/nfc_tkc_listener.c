#include <furi.h>
#include <furi_hal.h>

#include "nfc_tesla.h"

#include "nfc_tkc_listener.h"

#define TAG "NFC_TESLA_nfc_tkc_listener"

NfcTkcListener* nfc_tkc_listener_alloc(void* appd) {
    NfcTkcListener* instance = malloc(sizeof(NfcTkcListener));
    NfcTeslaApp* app = appd;
    instance->context = app;
    instance->nfc = app->nfc;
    return instance;
}

void nfc_tkc_listener_free(NfcTkcListener* instance) {
    furi_assert(instance);

    free(instance);
}

static NfcCommand tkc_listener_start_callback(NfcGenericEvent event, void* contextd) {
    NfcTkcListener* instance = contextd;
    NfcTkcListenerEvent result = {.type = NfcTkcListenerEventTypeNotDetected};
    UNUSED(event);

    FURI_LOG_D(TAG, "tkc_listener_start_callback()");
    instance->callback(result, instance->context);
    return NfcCommandContinue;
}

void nfc_tkc_listener_start(NfcTkcListener* instance, NfcTkcListenerCallback callback) {
    furi_assert(instance);
    furi_assert(callback);
    NfcTeslaApp* app = instance->context;
    furi_assert(app->nfc);
    furi_assert(app->nfc_device);

    instance->iso14443_4adata = nfc_device_get_data(app->nfc_device, NfcProtocolIso14443_4a);
    instance->listener =
        nfc_listener_alloc(instance->nfc, NfcProtocolIso14443_4a, instance->iso14443_4adata);

    instance->callback = callback;
    nfc_listener_start(instance->listener, tkc_listener_start_callback, instance);
}

void nfc_tkc_listener_stop(NfcTkcListener* instance) {
    furi_assert(instance);

    FURI_LOG_D(TAG, "nfc_listener_stop");
    nfc_listener_stop(instance->listener);
    FURI_LOG_D(TAG, "nfc_listener_free");
    nfc_listener_free(instance->listener);
    FURI_LOG_D(TAG, "nfc_listener_free done");
}
