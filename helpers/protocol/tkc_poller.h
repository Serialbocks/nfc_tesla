#pragma once

#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <bit_lib/bit_lib.h>

#include "tkc.h"

#define TKC_POLLER_MAX_BUFFER_SIZE (64U)

typedef enum {
    TkcPollerErrorNone,
    TkcPollerErrorTimeout,
    TkcPollerErrorProtocol,
} TkcPollerError;

typedef enum {
    TkcPollerEventTypeCardDetected,
    TkcPollerEventTypeRequestMode,

    TkcPollerEventTypeSuccess,
    TkcPollerEventTypeFail,
} TkcPollerEventType;

typedef enum {
    TkcPollerModeGetInfo,
} TkcPollerMode;

typedef struct {
    TkcPollerMode mode;
} TkcPollerEventDataRequestMode;

typedef union {
    TkcPollerEventDataRequestMode request_mode;
} TkcPollerEventData;

typedef struct {
    TkcPollerEventType type;
    TkcPollerEventData* data;
} TkcPollerEvent;

typedef enum {
    TkcPollerStateIdle,
    TkcPollerStateRequestMode,
    TkcPollerStateGetInfo,

    TkcPollerStateSuccess,
    TkcPollerStateFail,

    TkcPollerStateNum,
} TkcPollerState;

typedef NfcCommand (*TkcPollerCallback)(TkcPollerEvent event, void* context);

typedef struct TkcPoller {
    NfcPoller* poller;
    Iso14443_3aPoller* iso3_poller;
    TkcPollerState state;

    Tkc* tkc_data;

    TkcConfig config;

    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;

    uint16_t current_block;
    uint16_t total_blocks;

    NfcProtocol protocol;
    const NfcDeviceData* data;

    TkcPollerEvent tkc_event;
    TkcPollerEventData tkc_event_data;

    TkcPollerCallback callback;
    void* context;
} TkcPoller;

TkcPollerError tkc_poller_detect(Nfc* nfc, Tkc* gen4_data);

TkcPoller* tkc_poller_alloc(Nfc* nfc);

void tkc_poller_free(TkcPoller* instance);

void tkc_poller_start(TkcPoller* instance, TkcPollerCallback callback, void* context);

void tkc_poller_stop(TkcPoller* instance);