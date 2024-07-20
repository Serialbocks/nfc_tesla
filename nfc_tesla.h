#pragma once

#include <nfc/nfc_device.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_listener.h>
#include <nfc/helpers/nfc_data_generator.h>

#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view_dispatcher.h>
#include <gui/view_stack.h>
#include <gui/modules/menu.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_box.h>
#include <dialogs/dialogs.h>
#include <notification/notification_messages.h>

#include <storage/storage.h>
#include <stream/stream.h>
#include <stream/buffered_file_stream.h>
#include <toolbox/stream/file_stream.h>

#include "helpers/nfc_tkc_scanner.h"

typedef struct {
    TextBox* text_box_read;
    FuriString* text_box_read_text;
} NfcTeslaAppModel;

typedef struct {
    NfcTeslaAppModel* model;

    ViewDispatcher* view_dispatcher;
    Gui* gui;
    Storage* storage;
    NotificationApp* notifications;

    FuriThread* read_view_thread;

    Nfc* nfc;
    NfcPoller* poller;
    NfcDevice* nfc_device;

    NfcTkcScanner* scanner;
} NfcTeslaApp;
