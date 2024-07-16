#include "bit_buffer.h"
#include "core/check.h"
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/nfc_poller.h>
#include <bit_lib.h>
#include <string.h>

#include "tkc_poller.h"

typedef NfcCommand (*TkcPollerStateHandler)(TkcPoller* instance);

TkcPollerError tkc_poller_detect(Nfc* nfc, Tkc* gen4_data) {
    UNUSED(nfc);
    UNUSED(gen4_data);
    return TkcPollerErrorNone;
}

TkcPoller* tkc_poller_alloc(Nfc* nfc) {
    furi_assert(nfc);

    TkcPoller* instance = malloc(sizeof(TkcPoller));
    instance->poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_3a);

    instance->tkc_event.data = &instance->tkc_event_data;

    instance->tx_buffer = bit_buffer_alloc(TKC_POLLER_MAX_BUFFER_SIZE);
    instance->rx_buffer = bit_buffer_alloc(TKC_POLLER_MAX_BUFFER_SIZE);

    instance->tkc_data = tkc_alloc();

    return instance;
}

void tkc_poller_free(TkcPoller* instance) {
    furi_assert(instance);

    nfc_poller_free(instance->poller);

    bit_buffer_free(instance->tx_buffer);
    bit_buffer_free(instance->rx_buffer);

    tkc_free(instance->tkc_data);

    free(instance);
}

NfcCommand tkc_poller_idle_handler(TkcPoller* instance) {
    NfcCommand command = NfcCommandContinue;

    instance->current_block = 0;

    instance->tkc_event.type = TkcPollerEventTypeCardDetected;
    command = instance->callback(instance->tkc_event, instance->context);
    instance->state = TkcPollerStateRequestMode;

    return command;
}

NfcCommand tkc_poller_request_mode_handler(TkcPoller* instance) {
    NfcCommand command = NfcCommandContinue;

    instance->tkc_event.type = TkcPollerEventTypeRequestMode;
    command = instance->callback(instance->tkc_event, instance->context);

    if(instance->tkc_event_data.request_mode.mode == TkcPollerModeGetInfo) {
        instance->state = TkcPollerStateGetInfo;
    } else {
        instance->state = TkcPollerStateFail;
    }

    return command;
}

NfcCommand tkc_poller_get_info_handler(TkcPoller* instance) {
    NfcCommand command = NfcCommandContinue;
    UNUSED(instance);
    return command;
}

NfcCommand tkc_poller_success_handler(TkcPoller* instance) {
    NfcCommand command = NfcCommandContinue;

    instance->tkc_event.type = TkcPollerEventTypeSuccess;
    command = instance->callback(instance->tkc_event, instance->context);
    if(command != NfcCommandStop) {
        furi_delay_ms(100);
    }

    return command;
}

NfcCommand tkc_poller_fail_handler(TkcPoller* instance) {
    NfcCommand command = NfcCommandContinue;

    instance->tkc_event.type = TkcPollerEventTypeFail;
    command = instance->callback(instance->tkc_event, instance->context);
    if(command != NfcCommandStop) {
        furi_delay_ms(100);
    }

    return command;
}

static const TkcPollerStateHandler tkc_poller_state_handlers[TkcPollerStateNum] = {
    [TkcPollerStateIdle] = tkc_poller_idle_handler,
    [TkcPollerStateRequestMode] = tkc_poller_request_mode_handler,
    [TkcPollerStateGetInfo] = tkc_poller_get_info_handler,
    [TkcPollerStateSuccess] = tkc_poller_success_handler,
    [TkcPollerStateFail] = tkc_poller_fail_handler,
};

static NfcCommand tkc_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.protocol == NfcProtocolIso14443_3a);
    furi_assert(event.event_data);
    furi_assert(event.instance);

    NfcCommand command = NfcCommandContinue;
    TkcPoller* instance = context;
    instance->iso3_poller = event.instance;
    Iso14443_3aPollerEvent* iso3_event = event.event_data;

    if(iso3_event->type == Iso14443_3aPollerEventTypeReady) {
        command = tkc_poller_state_handlers[instance->state](instance);
    }

    return command;
}

void tkc_poller_start(TkcPoller* instance, TkcPollerCallback callback, void* context) {
    furi_assert(instance);
    furi_assert(callback);

    instance->callback = callback;
    instance->context = context;

    nfc_poller_start(instance->poller, tkc_poller_callback, instance);
}

void tkc_poller_stop(TkcPoller* instance) {
    furi_assert(instance);

    nfc_poller_stop(instance->poller);
}