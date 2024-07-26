#include <furi.h>
#include <furi_hal.h>
#include <toolbox/bit_buffer.h>

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
    Iso14443_4aListenerEvent* iso14443_event = event.event_data;

    size_t data_size_bits = bit_buffer_get_size(iso14443_event->data->buffer);
    size_t data_size_bytes = data_size_bits / 8;

    if(data_size_bits == 0) {
        FURI_LOG_D(TAG, "tkc_listener_start_callback() No data received!");
        return NfcCommandContinue;
    }

    const uint8_t* data = bit_buffer_get_data(iso14443_event->data->buffer);

    FURI_LOG_D(TAG, "tkc_listener_start_callback() event_type: %d", iso14443_event->type);
    FURI_LOG_D(TAG, "data size: %d bits (%d bytes)", data_size_bits, data_size_bytes);
    FuriString* debug_text = furi_string_alloc();
    furi_string_cat_printf(debug_text, "0x");
    for(uint8_t i = 0; i < data_size_bytes; i++) {
        furi_string_cat_printf(debug_text, "%02x", data[i]);
    }

    FURI_LOG_D(TAG, furi_string_get_cstr(debug_text));
    furi_string_free(debug_text);

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
