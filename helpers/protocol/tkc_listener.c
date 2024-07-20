#include <furi.h>

#include "nfc_tesla.h"
#include "tkc_listener_i.h"

TkcListener* tkc_listener_alloc(NfcTeslaApp* app) {
    TkcListener* instance = malloc(sizeof(TkcListener));
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

void tkc_listener_start(TkcListener* instance) {
    furi_assert(instance);
}

void tkc_listener_stop(TkcListener* instance) {
    furi_assert(instance);
}
