#pragma once

#include "core/common_defines.h"
#include <stdint.h>

#define TKC_PUBLIC_KEY_SIZE (65)
#define TKC_ATQA_LEN (2)

#define TKC_APDU_PREFIX (0x80)
#define TKC_APDU_MAX_LC_LENGTH (3)
#define TKC_APDU_MAX_LE_LENGTH (3)
#define TKC_APDU_MAX_DATA_LENGTH (81)
#define TKC_APDU_MIN_LENGTH (4)
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

typedef struct {
    uint16_t form_factor;
    uint8_t public_key[TKC_PUBLIC_KEY_SIZE];
} Tkc;

Tkc* tkc_alloc();

void tkc_free(Tkc* instance);

void tkc_reset(Tkc* instance);

void tkc_copy(Tkc* dest, const Tkc* source);