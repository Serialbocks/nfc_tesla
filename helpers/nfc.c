
#include <nfc/protocols/nfc_poller_base.h>

#include "nfc.h"

static Iso14443_3aPoller* iso14443_3a_poller_alloc(Nfc* nfc) {
    furi_assert(nfc);

    Iso14443_3aPoller* instance = malloc(sizeof(Iso14443_3aPoller));
    instance->nfc = nfc;
    instance->tx_buffer = bit_buffer_alloc(ISO14443_3A_POLLER_MAX_BUFFER_SIZE);
    instance->rx_buffer = bit_buffer_alloc(ISO14443_3A_POLLER_MAX_BUFFER_SIZE);

    nfc_config(instance->nfc, NfcModePoller, NfcTechIso14443a);
    nfc_set_guard_time_us(instance->nfc, ISO14443_3A_GUARD_TIME_US);
    nfc_set_fdt_poll_fc(instance->nfc, ISO14443_3A_FDT_POLL_FC);
    nfc_set_fdt_poll_poll_us(instance->nfc, ISO14443_3A_POLL_POLL_MIN_US);
    instance->data = iso14443_3a_alloc();

    instance->iso14443_3a_event.data = &instance->iso14443_3a_event_data;
    instance->general_event.protocol = NfcProtocolIso14443_3a;
    instance->general_event.event_data = &instance->iso14443_3a_event;
    instance->general_event.instance = instance;

    return instance;
}

static void iso14443_3a_poller_free_new(Iso14443_3aPoller* iso14443_3a_poller) {
    furi_assert(iso14443_3a_poller);

    Iso14443_3aPoller* instance = iso14443_3a_poller;
    furi_assert(instance->tx_buffer);
    furi_assert(instance->rx_buffer);
    furi_assert(instance->data);

    bit_buffer_free(instance->tx_buffer);
    bit_buffer_free(instance->rx_buffer);
    iso14443_3a_free(instance->data);
    free(instance);
}

Iso14443_4aPoller* poller_alloc(Nfc* nfc) {
    Iso14443_4aPoller* instance = malloc(sizeof(Iso14443_4aPoller));
    instance->iso14443_3a_poller = iso14443_3a_poller_alloc(nfc);
    instance->data = iso14443_4a_alloc();
    instance->iso14443_4_layer = iso14443_4_layer_alloc();
    instance->tx_buffer = bit_buffer_alloc(ISO14443_4A_POLLER_BUF_SIZE);
    instance->rx_buffer = bit_buffer_alloc(ISO14443_4A_POLLER_BUF_SIZE);

    instance->iso14443_4a_event.data = &instance->iso14443_4a_event_data;

    instance->general_event.protocol = NfcProtocolIso14443_4a;
    instance->general_event.event_data = &instance->iso14443_4a_event;
    instance->general_event.instance = instance;

    return instance;
}

void poller_free(Iso14443_4aPoller* instance) {
    furi_assert(instance);

    iso14443_4a_free(instance->data);
    iso14443_4_layer_free(instance->iso14443_4_layer);
    bit_buffer_free(instance->tx_buffer);
    bit_buffer_free(instance->rx_buffer);
    iso14443_3a_poller_free_new(instance->iso14443_3a_poller);
    free(instance);
}

static NfcCommand read_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolIso14443_4a);

    NfcTeslaApp* instance = context;
    const Iso14443_4aPollerEvent* iso14443_4a_event = event.event_data;

    if(iso14443_4a_event->type == Iso14443_4aPollerEventTypeReady) {
        nfc_device_set_data(
            instance->nfc_device, NfcProtocolIso14443_4a, nfc_poller_get_data(instance->poller));
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

void read_card(NfcTeslaApp* instance) {
    nfc_poller_start(instance->poller, read_poller_callback, instance);
    //nfc_protocol_support[protocol]->scene_read.on_enter(instance);
    //nfc_blink_read_start(instance);
}