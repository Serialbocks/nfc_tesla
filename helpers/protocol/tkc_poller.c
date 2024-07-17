#include <furi/furi.h>
#include <furi_hal.h>

#include "bit_buffer.h"
#include "core/check.h"
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/nfc_poller.h>
#include <bit_lib.h>
#include <string.h>

#include "tkc_poller.h"

#define TAG "NFC_TESLA_tkc_poller"
#define TKC_POLLER_THREAD_FLAG_DETECTED (1U << 0)

typedef NfcCommand (*TkcPollerStateHandler)(TkcPoller* instance);

static TkcApduCommand tkc_apdu_get_form_factor =
    {.ins = 0x14, .p1 = 0x00, .p2 = 0x00, .lc_len = 0, .data = NULL, .le_len = 1, .le = {0x00}};

typedef struct {
    NfcPoller* poller;
    Tkc tkc_data;
    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;
    FuriThreadId thread_id;
    TkcPollerError error;
} TkcPollerDetectContext;

static TkcPollerError tkc_send_apdu_command(
    TkcPollerDetectContext* tkc_poller_detect_ctx,
    Iso14443_3aPoller* iso3_poller,
    TkcApduCommand* instruction) {
    uint8_t instruction_len = TKC_APDU_MIN_LENGTH;
    uint16_t data_len;

    // Add the header
    bit_buffer_append_byte(tkc_poller_detect_ctx->tx_buffer, TKC_APDU_PREFIX);
    bit_buffer_append_byte(tkc_poller_detect_ctx->tx_buffer, instruction->ins);
    bit_buffer_append_byte(tkc_poller_detect_ctx->tx_buffer, instruction->p1);
    bit_buffer_append_byte(tkc_poller_detect_ctx->tx_buffer, instruction->p2);

    if(instruction->lc_len > 0) {
        // Add Lc
        instruction_len += instruction->lc_len;
        bit_buffer_append_bytes(
            tkc_poller_detect_ctx->tx_buffer, instruction->lc, instruction->lc_len);

        // Add command data
        if(instruction->lc_len == 1) {
            instruction_len += instruction->lc[0];
            bit_buffer_append_bytes(
                tkc_poller_detect_ctx->tx_buffer, instruction->data, instruction->lc[0]);
        } else if(instruction->lc_len == 3) {
            memcpy(&data_len, &(instruction->lc[1]), 2);
            bit_buffer_append_bytes(tkc_poller_detect_ctx->tx_buffer, instruction->data, data_len);
        } else {
            return TkcPollerErrorProtocol;
        }
    }

    if(instruction->le_len > 0) {
        instruction_len += instruction->le_len;
        bit_buffer_append_bytes(
            tkc_poller_detect_ctx->tx_buffer, instruction->le, instruction->le_len);
    }

    Iso14443_3aError error = iso14443_3a_poller_send_standard_frame(
        iso3_poller,
        tkc_poller_detect_ctx->tx_buffer,
        tkc_poller_detect_ctx->rx_buffer,
        TKC_POLLER_MAX_FWT);

    if(error != Iso14443_3aErrorNone) {
        FURI_LOG_D(TAG, "Iso14443_3aError %u", error);
        return TkcPollerErrorProtocol;
    }

    return TkcPollerErrorNone;
}

NfcCommand tkc_poller_detect_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.protocol == NfcProtocolIso14443_3a);
    furi_assert(event.instance);
    furi_assert(event.event_data);

    NfcCommand command = NfcCommandStop;
    TkcPollerDetectContext* tkc_poller_detect_ctx = context;
    Iso14443_3aPoller* iso3_poller = event.instance;
    Iso14443_3aPollerEvent* iso3_event = event.event_data;
    tkc_poller_detect_ctx->error = TkcPollerErrorTimeout;

    if(iso3_event->type == Iso14443_3aPollerEventTypeReady) {
        do {
            // send get_form_factor command
            TkcPollerError error = tkc_send_apdu_command(
                tkc_poller_detect_ctx, iso3_poller, &tkc_apdu_get_form_factor);

            if(error != TkcPollerErrorNone) {
                tkc_poller_detect_ctx->error = TkcPollerErrorProtocol;
                break;
            }
            size_t rx_bytes = bit_buffer_get_size_bytes(tkc_poller_detect_ctx->rx_buffer);
            if(rx_bytes != sizeof(tkc_poller_detect_ctx->tkc_data.form_factor)) {
                tkc_poller_detect_ctx->error = TkcPollerErrorProtocol;
                break;
            }

            memcpy(
                &tkc_poller_detect_ctx->tkc_data.form_factor,
                bit_buffer_get_data(tkc_poller_detect_ctx->rx_buffer),
                sizeof(tkc_poller_detect_ctx->tkc_data.form_factor));

            // check revision
            bit_buffer_reset(tkc_poller_detect_ctx->tx_buffer);
            bit_buffer_reset(tkc_poller_detect_ctx->rx_buffer);

        } while(false);
    } else if(iso3_event->type == Iso14443_3aPollerEventTypeError) {
        tkc_poller_detect_ctx->error = TkcPollerErrorTimeout;
    }

    furi_thread_flags_set(tkc_poller_detect_ctx->thread_id, TKC_POLLER_THREAD_FLAG_DETECTED);

    return command;
}

TkcPollerError tkc_poller_detect(Nfc* nfc, Tkc* tkc_data) {
    furi_assert(nfc);

    TkcPollerDetectContext tkc_poller_detect_ctx = {};
    tkc_poller_detect_ctx.poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_3a);
    tkc_poller_detect_ctx.tx_buffer = bit_buffer_alloc(TKC_POLLER_MAX_BUFFER_SIZE);
    tkc_poller_detect_ctx.rx_buffer = bit_buffer_alloc(TKC_POLLER_MAX_BUFFER_SIZE);
    tkc_poller_detect_ctx.thread_id = furi_thread_get_current_id();
    tkc_poller_detect_ctx.error = TkcPollerErrorNone;

    nfc_poller_start(
        tkc_poller_detect_ctx.poller, tkc_poller_detect_callback, &tkc_poller_detect_ctx);
    uint32_t flags =
        furi_thread_flags_wait(TKC_POLLER_THREAD_FLAG_DETECTED, FuriFlagWaitAny, FuriWaitForever);
    if(flags & TKC_POLLER_THREAD_FLAG_DETECTED) {
        furi_thread_flags_clear(TKC_POLLER_THREAD_FLAG_DETECTED);
    }
    nfc_poller_stop(tkc_poller_detect_ctx.poller);

    nfc_poller_free(tkc_poller_detect_ctx.poller);
    bit_buffer_free(tkc_poller_detect_ctx.tx_buffer);
    bit_buffer_free(tkc_poller_detect_ctx.rx_buffer);

    if(tkc_poller_detect_ctx.error == TkcPollerErrorNone) {
        tkc_copy(tkc_data, &tkc_poller_detect_ctx.tkc_data);
    }

    return tkc_poller_detect_ctx.error;
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