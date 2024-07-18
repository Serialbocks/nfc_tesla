#pragma once

#include "core/common_defines.h"
#include <stdint.h>

#define TKC_ATQA_LEN (2)

#define TKC_APDU_PREFIX                  (0x80)
#define TKC_APDU_MAX_LC_LENGTH           (3)
#define TKC_APDU_MAX_LE_LENGTH           (3)
#define TKC_APDU_MAX_DATA_LENGTH         (81)
#define TKC_APDU_MIN_LENGTH              (4)
#define TKC_APDU_RESPONSE_TRAILER_LENGTH (2)

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

#define TKC_PUBLIC_KEY_SIZE    (65)
#define TKC_PUBLIC_KEY_EC_SIZE (32)
typedef union {
    uint8_t data_raw[TKC_PUBLIC_KEY_SIZE];
    struct {
        uint8_t byte_1; // Always 0x04
        uint8_t public_key[TKC_PUBLIC_KEY_SIZE - 1];
    } FURI_PACKED data_parsed;
} TkcPublicKey;

#define TKC_VERSION_INFO_SIZE (6)
typedef union {
    uint8_t data_raw[TKC_VERSION_INFO_SIZE];
    struct {
        uint16_t tesla_logic_version;
        uint16_t tesla_store_version;
        uint16_t unknown;
    } FURI_PACKED data_parsed;
} TkcVersionInfo;

#define TKC_AUTHENTICATION_CHALLENGE_SIZE (16)
#define TKC_AUTHENTICATION_RESPONSE_SIZE  TKC_AUTHENTICATION_CHALLENGE_SIZE

typedef struct {
    TkcPublicKey public_key;
    TkcVersionInfo version_info;
    uint16_t form_factor;
    uint8_t auth_challenge[TKC_AUTHENTICATION_CHALLENGE_SIZE];
    uint8_t auth_challenge_result[TKC_AUTHENTICATION_CHALLENGE_SIZE];
} Tkc;

Tkc* tkc_alloc();

void tkc_free(Tkc* instance);

void tkc_reset(Tkc* instance);

void tkc_copy(Tkc* dest, const Tkc* source);
