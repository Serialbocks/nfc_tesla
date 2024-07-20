#pragma once

#include "tkc_listener.h"

#include <nfc/nfc_listener.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_listener.h>

struct TkcListener {
    Nfc* nfc;
    NfcListener* listener;
    Iso14443_4aData* iso14443_4adata;
};
