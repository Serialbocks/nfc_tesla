#pragma once

#include <nfc/nfc_device.h>
#include <nfc/nfc_poller.h>

#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/menu.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_box.h>
#include <dialogs/dialogs.h>

#include <storage/storage.h>
#include <stream/stream.h>
#include <stream/buffered_file_stream.h>
#include <toolbox/stream/file_stream.h>

typedef struct {
    int32_t unused;
} NfcTeslaAppModel;

typedef struct {
    NfcTeslaAppModel* model;

    ViewDispatcher* view_dispatcher;
    Gui* gui;
    Storage* storage;

    FuriThread* debug_view_thread;

    Nfc* nfc;
    NfcDevice* nfc_device;
    NfcPoller* poller;
} NfcTeslaApp;