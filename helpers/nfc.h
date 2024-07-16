#pragma once

#include <nfc.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller_i.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_listener.h>

#include "../nfc_tesla.h"

#define ISO14443_4A_POLLER_BUF_SIZE (256U)

Iso14443_4aPoller* poller_alloc(Nfc* nfc);
void poller_free(Iso14443_4aPoller* instance);

void read_card(NfcTeslaApp* instance);