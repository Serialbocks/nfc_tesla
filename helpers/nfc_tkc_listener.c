#include <furi.h>
#include <furi_hal.h>
#include <toolbox/bit_buffer.h>

#include "nfc_tesla.h"

#include "nfc_tkc_listener.h"

#define TAG "NFC_TESLA_nfc_tkc_listener"

#define NFC_TKC_LISTENER_MAX_BUFFER_SIZE 256
#define NFC_TKC_PUBLIC_KEY_SIZE          64
#define NFC_TKC_PRIVATE_KEY_SIZE         32

NfcTkcListener* nfc_tkc_listener_alloc(void* appd) {
    NfcTkcListener* instance = malloc(sizeof(NfcTkcListener));
    NfcTeslaApp* app = appd;
    instance->context = app;
    instance->nfc = app->nfc;
    instance->tx_buffer = bit_buffer_alloc(NFC_TKC_LISTENER_MAX_BUFFER_SIZE);
    instance->public_key = malloc(NFC_TKC_PUBLIC_KEY_SIZE);
    instance->private_key = malloc(NFC_TKC_PRIVATE_KEY_SIZE);
    p256_gen_keypair(instance->private_key, instance->public_key);
    return instance;
}

void nfc_tkc_listener_free(NfcTkcListener* instance) {
    furi_assert(instance);

    free(instance->private_key);
    free(instance->public_key);
    bit_buffer_free(instance->tx_buffer);
    free(instance);
}

static NfcTkcListenerEventType tkc_respond_to_command(
    NfcTkcListener* instance,
    Iso14443_3aListener* iso14443_listener,
    BitBuffer* rx_buffer,
    Iso14443_4aListenerEvent* iso14443_event) {
    BitBuffer* tx_buffer = instance->tx_buffer;

    const uint8_t* data = bit_buffer_get_data(rx_buffer);
    size_t data_size_bits = bit_buffer_get_size(rx_buffer);
    size_t data_size_bytes = data_size_bits / 8;

    if(data_size_bits < 3) {
        FURI_LOG_W(TAG, "tkc_respond_to_command() No data received!");
        return NfcTkcListenerEventTypeNotDetected;
    }

    // Debug print data received
    FURI_LOG_D(TAG, "tkc_respond_to_command() event_type: %d", iso14443_event->type);
    FURI_LOG_D(TAG, "data size: %d bits (%d bytes)", data_size_bits, data_size_bytes);
    FuriString* debug_text = furi_string_alloc();
    furi_string_cat_printf(debug_text, "0x");
    for(uint8_t i = 0; i < data_size_bytes; i++) {
        furi_string_cat_printf(debug_text, "%02x", data[i]);
    }
    FURI_LOG_D(TAG, furi_string_get_cstr(debug_text));
    furi_string_free(debug_text);

    bool response_required = false;
    if(data[1] == TKC_APDU_PREFIX) {
        uint8_t ins = data[2];
        bit_buffer_reset(tx_buffer);
        bit_buffer_append_byte(tx_buffer, data[0]);
        switch(ins) {
        case TKC_APDU_GET_FORM_FACTOR_INS:
            response_required = true;
            bit_buffer_append_byte(tx_buffer, 0x00);
            bit_buffer_append_byte(tx_buffer, 0x01);
            break;
        case TKC_APDU_GET_PUBLIC_KEY_INS:
            response_required = true;
            break;
        default:
            break;
        }
    }

    if(response_required) {
        bit_buffer_append_byte(tx_buffer, 0x90);
        bit_buffer_append_byte(tx_buffer, 0x00);
        iso14443_crc_append(Iso14443CrcTypeA, tx_buffer);

        NfcError error = nfc_listener_tx((Nfc*)iso14443_listener, tx_buffer);
        if(error != NfcErrorNone) {
            FURI_LOG_W(TAG, "Tx error: %d", error);
            return NfcTkcListenerEventTypeError;
        }
    }

    return NfcTkcListenerEventTypeDetected;
}

static NfcCommand tkc_listener_start_callback(NfcGenericEvent event, void* contextd) {
    NfcTkcListener* instance = contextd;
    Iso14443_4aListenerEvent* iso14443_event = event.event_data;
    Iso14443_3aListener* iso14443_listener = event.instance;
    NfcTkcListenerEvent result;

    result.type = tkc_respond_to_command(
        instance, iso14443_listener, iso14443_event->data->buffer, iso14443_event);
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
