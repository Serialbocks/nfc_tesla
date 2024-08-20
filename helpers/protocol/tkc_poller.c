#include <furi/furi.h>
#include <furi_hal.h>

#include "bit_buffer.h"
#include "core/check.h"
#include <nfc/protocols/Iso14443_4a/Iso14443_4a.h>
#include <nfc/protocols/Iso14443_4a/Iso14443_4a_poller.h>
#include <nfc/protocols/Iso14443_4a/Iso14443_4a_poller_i.h>
#include <nfc/nfc_poller.h>
#include <bit_lib.h>
#include <string.h>

#include "tkc_poller.h"

#define TAG                             "NFC_TESLA_tkc_poller"
#define TKC_POLLER_THREAD_FLAG_DETECTED (1U << 0)

typedef NfcCommand (*TkcPollerStateHandler)(TkcPoller* instance);

/**
 * Commands from https://gist.github.com/darconeous/2cd2de11148e3a75685940158bddf933
 * saved locally as tesla-key-card-protocol.md
 */
static TkcApduCommand tkc_apdu_get_public_key = {
    .ins = TKC_APDU_GET_PUBLIC_KEY_INS,
    .p1 = 0x00,
    .p2 = 0x00,
    .lc_len = 0,
    .data = NULL,
    .le_len = 1,
    .le = {0x00}};

static TkcApduCommand tkc_apdu_get_version_info = {
    .ins = TKC_APDU_GET_VERSION_INFO_INS,
    .p1 = 0x00,
    .p2 = 0x00,
    .lc_len = 0,
    .data = NULL,
    .le_len = 1,
    .le = {0x00}};

static TkcApduCommand tkc_apdu_get_form_factor = {
    .ins = TKC_APDU_GET_FORM_FACTOR_INS,
    .p1 = 0x00,
    .p2 = 0x00,
    .lc_len = 0,
    .data = NULL,
    .le_len = 1,
    .le = {0x00}};

#define TKC_APDU_AUTHENTICATION_PUBLIC_KEY_LEN 65
#define TKC_APDU_AUTHENTICATION_CHALLENGE_LEN  16
#define TKC_APDU_AUTHENTICATION_DATA_LEN       81 // sum of public key and challenge
static TkcApduCommand tkc_apdu_authentication_challenge = {
    .ins = TCK_APDU_AUTHENTICATION_CHALLENGE_INS,
    .p1 = 0x00,
    .p2 = 0x00,
    .lc_len = 1,
    .lc = {0x51},
    .le_len = 1,
    .le = {0x00}};

typedef struct {
    NfcPoller* poller;
    Tkc* tkc_data;
    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;
    FuriThreadId thread_id;
    TkcPollerError error;
    uint8_t* public_key;
    uint8_t* private_key;
} TkcPollerDetectContext;

static TkcPollerError tkc_send_apdu_command(
    TkcPollerDetectContext* tkc_poller_detect_ctx,
    Iso14443_4aPoller* iso4_poller,
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

    FURI_LOG_D(TAG, "Send block instruction_len: %u", instruction_len);
    Iso14443_4aError error = iso14443_4a_poller_send_block(
        iso4_poller, tkc_poller_detect_ctx->tx_buffer, tkc_poller_detect_ctx->rx_buffer);

    if(error != Iso14443_4aErrorNone) {
        FURI_LOG_D(TAG, "Iso14443_4aError %u for command 0x%02x", error, instruction->ins);
        return TkcPollerErrorProtocol;
    }

    return TkcPollerErrorNone;
}

NfcCommand tkc_poller_detect_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.protocol == NfcProtocolIso14443_4a);
    furi_assert(event.instance);
    furi_assert(event.event_data);

    NfcCommand command = NfcCommandStop;
    TkcPollerDetectContext* tkc_poller_detect_ctx = context;
    Iso14443_4aPoller* iso4_poller = event.instance;
    Iso14443_4aPollerEvent* iso4_event = event.event_data;
    tkc_poller_detect_ctx->error = TkcPollerErrorTimeout;

    if(iso4_event->type == Iso14443_4aPollerEventTypeReady) {
        do {
            // send get_form_factor command
            TkcPollerError error = tkc_send_apdu_command(
                tkc_poller_detect_ctx, iso4_poller, &tkc_apdu_get_form_factor);

            if(error != TkcPollerErrorNone) {
                tkc_poller_detect_ctx->error = TkcPollerErrorProtocol;
                break;
            }
            size_t rx_bytes = bit_buffer_get_size_bytes(tkc_poller_detect_ctx->rx_buffer);
            if(rx_bytes != sizeof(tkc_poller_detect_ctx->tkc_data->form_factor) +
                               TKC_APDU_RESPONSE_TRAILER_LENGTH) {
                FURI_LOG_D(TAG, "form_factor rx_bytes length: %u", rx_bytes);
                tkc_poller_detect_ctx->error = TkcPollerErrorProtocol;
                break;
            }

            tkc_poller_detect_ctx->error = TkcPollerErrorNone;
            tkc_poller_detect_ctx->tkc_data->form_factor =
                *((uint16_t*)bit_buffer_get_data(tkc_poller_detect_ctx->rx_buffer));

            // get public key
            bit_buffer_reset(tkc_poller_detect_ctx->tx_buffer);
            bit_buffer_reset(tkc_poller_detect_ctx->rx_buffer);
            error = tkc_send_apdu_command(
                tkc_poller_detect_ctx, iso4_poller, &tkc_apdu_get_public_key);

            if(error != TkcPollerErrorNone) {
                tkc_poller_detect_ctx->error = TkcPollerErrorProtocol;
                break;
            }

            rx_bytes = bit_buffer_get_size_bytes(tkc_poller_detect_ctx->rx_buffer);
            if(rx_bytes != TKC_PUBLIC_KEY_SIZE + TKC_APDU_RESPONSE_TRAILER_LENGTH) {
                FURI_LOG_D(TAG, "public_key rx_bytes length: %u", rx_bytes);
                tkc_poller_detect_ctx->error = TkcPollerErrorProtocol;
                break;
            }

            memcpy(
                tkc_poller_detect_ctx->tkc_data->public_key.data_raw,
                bit_buffer_get_data(tkc_poller_detect_ctx->rx_buffer),
                TKC_PUBLIC_KEY_SIZE);

            // get version info
            bit_buffer_reset(tkc_poller_detect_ctx->tx_buffer);
            bit_buffer_reset(tkc_poller_detect_ctx->rx_buffer);
            error = tkc_send_apdu_command(
                tkc_poller_detect_ctx, iso4_poller, &tkc_apdu_get_version_info);

            if(error != TkcPollerErrorNone) {
                tkc_poller_detect_ctx->error = TkcPollerErrorProtocol;
                break;
            }

            rx_bytes = bit_buffer_get_size_bytes(tkc_poller_detect_ctx->rx_buffer);
            if(rx_bytes != TKC_VERSION_INFO_SIZE + TKC_APDU_RESPONSE_TRAILER_LENGTH) {
                FURI_LOG_D(TAG, "version_info rx_bytes length: %u", rx_bytes);
                tkc_poller_detect_ctx->error = TkcPollerErrorProtocol;
                break;
            }

            memcpy(
                tkc_poller_detect_ctx->tkc_data->version_info.data_raw,
                bit_buffer_get_data(tkc_poller_detect_ctx->rx_buffer),
                TKC_VERSION_INFO_SIZE);

            // perform authentication challenge
            // First 64 bytes is the public key
            uint8_t auth_challenge_data[TKC_APDU_AUTHENTICATION_DATA_LEN];
            auth_challenge_data[0] = 0x04;
            memcpy(&(auth_challenge_data[1]), tkc_poller_detect_ctx->public_key, 64);

            // Calculate ECDH shared secret
            uint8_t shared_secret[32];
            int ecdh_result = p256_ecdh_shared_secret(
                shared_secret,
                tkc_poller_detect_ctx->private_key,
                tkc_poller_detect_ctx->tkc_data->public_key.data_parsed.public_key);
            if(ecdh_result != 0) {
                tkc_poller_detect_ctx->error = TkcPollerErrorProtocol;
                FURI_LOG_D(TAG, "failure to calculate shared secret: %u", ecdh_result);
                break;
            }

            // Calculate the key from the sha1 of the shared key
            uint8_t sha1[20];
            mbedtls_sha1(shared_secret, 32, sha1);

            furi_hal_random_fill_buf(
                &(auth_challenge_data[TKC_APDU_AUTHENTICATION_PUBLIC_KEY_LEN]),
                TKC_APDU_AUTHENTICATION_CHALLENGE_LEN);
            tkc_apdu_authentication_challenge.data = auth_challenge_data;
            memcpy(
                tkc_poller_detect_ctx->tkc_data->auth_challenge,
                &(auth_challenge_data[TKC_APDU_AUTHENTICATION_PUBLIC_KEY_LEN]),
                TKC_APDU_AUTHENTICATION_CHALLENGE_LEN);

            bit_buffer_reset(tkc_poller_detect_ctx->tx_buffer);
            bit_buffer_reset(tkc_poller_detect_ctx->rx_buffer);
            error = tkc_send_apdu_command(
                tkc_poller_detect_ctx, iso4_poller, &tkc_apdu_authentication_challenge);

            if(error != TkcPollerErrorNone) {
                tkc_poller_detect_ctx->error = TkcPollerErrorProtocol;
                break;
            }

            rx_bytes = bit_buffer_get_size_bytes(tkc_poller_detect_ctx->rx_buffer);
            if(rx_bytes != TKC_AUTHENTICATION_RESPONSE_SIZE + TKC_APDU_RESPONSE_TRAILER_LENGTH) {
                FURI_LOG_D(TAG, "auth_challenge rx_bytes length: %u", rx_bytes);
                tkc_poller_detect_ctx->error = TkcPollerErrorProtocol;
                break;
            }

            // Decrypt the response and check
            uint8_t auth_response[TKC_AUTHENTICATION_RESPONSE_SIZE];
            memcpy(
                auth_response,
                bit_buffer_get_data(tkc_poller_detect_ctx->rx_buffer),
                TKC_AUTHENTICATION_RESPONSE_SIZE);

            mbedtls_aes_context aes_context;
            mbedtls_aes_init(&aes_context);
            mbedtls_aes_setkey_dec(&aes_context, sha1, 128);
            mbedtls_aes_crypt_ecb(
                &aes_context,
                MBEDTLS_AES_DECRYPT,
                auth_response,
                tkc_poller_detect_ctx->tkc_data->auth_challenge_result);
            mbedtls_aes_free(&aes_context);

            // compare decrypted response with original challenge
            int cmp_result = memcmp(
                tkc_poller_detect_ctx->tkc_data->auth_challenge,
                tkc_poller_detect_ctx->tkc_data->auth_challenge_result,
                TKC_APDU_AUTHENTICATION_CHALLENGE_LEN);
            tkc_poller_detect_ctx->tkc_data->auth_challenge_is_successful = (cmp_result == 0);

            // copy Iso14443_4aData data to return object
            iso14443_4a_copy(tkc_poller_detect_ctx->tkc_data->iso14443_4a_data, iso4_poller->data);

        } while(false);
    } else if(iso4_event->type == Iso14443_4aPollerEventTypeError) {
        tkc_poller_detect_ctx->error = TkcPollerErrorTimeout;
    }

    furi_thread_flags_set(tkc_poller_detect_ctx->thread_id, TKC_POLLER_THREAD_FLAG_DETECTED);

    return command;
}

TkcPollerError tkc_poller_detect(Nfc* nfc, Tkc* tkc_data) {
    furi_assert(nfc);

    TkcPollerDetectContext tkc_poller_detect_ctx = {};
    tkc_poller_detect_ctx.poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_4a);
    tkc_poller_detect_ctx.tx_buffer = bit_buffer_alloc(TKC_POLLER_MAX_BUFFER_SIZE);
    tkc_poller_detect_ctx.rx_buffer = bit_buffer_alloc(TKC_POLLER_MAX_BUFFER_SIZE);
    tkc_poller_detect_ctx.tkc_data = tkc_data;
    tkc_poller_detect_ctx.thread_id = furi_thread_get_current_id();
    tkc_poller_detect_ctx.error = TkcPollerErrorNone;
    tkc_poller_detect_ctx.public_key = malloc(64 * sizeof(uint8_t));
    tkc_poller_detect_ctx.private_key = malloc(32 * sizeof(uint8_t));
    p256_gen_keypair(tkc_poller_detect_ctx.private_key, tkc_poller_detect_ctx.public_key);

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
    free(tkc_poller_detect_ctx.public_key);
    free(tkc_poller_detect_ctx.private_key);

    return tkc_poller_detect_ctx.error;
}

TkcPoller* tkc_poller_alloc(Nfc* nfc) {
    furi_assert(nfc);

    TkcPoller* instance = malloc(sizeof(TkcPoller));
    instance->poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_4a);

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
    furi_assert(event.protocol == NfcProtocolIso14443_4a);
    furi_assert(event.event_data);
    furi_assert(event.instance);

    NfcCommand command = NfcCommandContinue;
    TkcPoller* instance = context;
    instance->iso3_poller = event.instance;
    Iso14443_4aPollerEvent* iso3_event = event.event_data;

    if(iso3_event->type == Iso14443_4aPollerEventTypeReady) {
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
