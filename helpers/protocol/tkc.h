#pragma once

#include "core/common_defines.h"
#include <stdint.h>

#define TKC_CONFIG_SIZE (4)
#define TKC_ATQA_LEN (2)

typedef union {
    uint8_t data_raw[TKC_CONFIG_SIZE];
    struct {
        uint8_t ats_len;
        //uint8_t ats[TKC_ATS_MAX_LEN]; // mb another class?
        uint8_t atqa[TKC_ATQA_LEN];
        uint8_t sak;

    } FURI_PACKED data_parsed;
} TkcConfig;

typedef struct {
    TkcConfig config;
} Tkc;

Tkc* tkc_alloc();

void tkc_free(Tkc* instance);

void tkc_reset(Tkc* instance);

void tkc_copy(Tkc* dest, const Tkc* source);