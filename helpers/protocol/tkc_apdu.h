#pragma once

#include "core/common_defines.h"
#include <stdint.h>

/**
 * Commands from https://gist.github.com/darconeous/2cd2de11148e3a75685940158bddf933
 * saved locally as tesla-key-card-protocol.md
 */
#define TKC_APDU_PREFIX (0x80)
#define TKC_APDU_MAX_LC_LENGTH (3)
#define TKC_APDU_MAX_LE_LENGTH (3)
#define TKC_APDU_MAX_DATA_LENGTH (81)
#define TKC_APDU_MIN_LENGTH (4)

typedef struct TkcApduCommand {
    uint8_t ins;
    uint8_t p1;
    uint8_t p2;
    uint8_t lc_len;
    uint8_t lc[TKC_APDU_MAX_LC_LENGTH];
    uint8_t* data;
    uint8_t le_len;
    uint8_t le[TKC_APDU_MAX_LE_LENGTH];
} TkcApduCommand;
