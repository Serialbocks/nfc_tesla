#include <furi.h>
#include <furi_hal.h>

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

#include "icons.h"

#define TAG "NFC_TESLA_main"

#define VIEW_DISPATCHER_MENU 0
#define VIEW_DISPATCHER_DEBUG 1

typedef struct {
    int32_t unused;
} NfcTeslaAppModel;

typedef struct {
    NfcTeslaAppModel* model;

    ViewDispatcher* view_dispatcher;
    Gui* gui;
    Storage* storage;

    FuriThread* debug_view_thread;
} NfcTeslaApp;

NfcTeslaApp* app;
TextBox* textBoxDebug;
FuriString* textBoxDebugText;
Popup* popup;
FuriMessageQueue* event_queue;

int32_t debug_view_thread(void* contextd) {
    NfcTeslaApp* context = contextd;
    InputEvent input_event;
    FuriStatus furi_status;

    while(true) {
        furi_status = furi_message_queue_get(event_queue, &input_event, 100);
        if(furi_status != FuriStatusOk || input_event.type != InputTypePress) {
            continue;
        }
        if(input_event.key == InputKeyBack) {
            view_dispatcher_switch_to_view(context->view_dispatcher, VIEW_DISPATCHER_MENU);
            return 0;
        }
    }
}

void debug_printf(const char format[], ...) {
    va_list args;
    va_start(args, format);
    furi_string_vprintf(textBoxDebugText, format, args);
    text_box_set_text(textBoxDebug, furi_string_get_cstr(textBoxDebugText));
    va_end(args);
}

static void dispatch_view(void* contextd, uint32_t index) {
    NfcTeslaApp* context = (NfcTeslaApp*)contextd;

    if(index == VIEW_DISPATCHER_DEBUG) {
        view_dispatcher_switch_to_view(context->view_dispatcher, VIEW_DISPATCHER_DEBUG);
        furi_thread_start(context->debug_view_thread);
    }
}

static NfcTeslaApp* nfcTeslaApp_alloc() {
    FURI_LOG_D(TAG, "alloc");
    NfcTeslaApp* instance = malloc(sizeof(NfcTeslaApp));

    instance->model = malloc(sizeof(NfcTeslaAppModel));
    memset(instance->model, 0x0, sizeof(NfcTeslaAppModel));

    instance->view_dispatcher = view_dispatcher_alloc();

    instance->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_enable_queue(instance->view_dispatcher);
    view_dispatcher_attach_to_gui(
        instance->view_dispatcher, instance->gui, ViewDispatcherTypeFullscreen);

    instance->storage = furi_record_open(RECORD_STORAGE);

    instance->debug_view_thread =
        furi_thread_alloc_ex("debug_view_thread", 512, debug_view_thread, instance);

    return instance;
}

static void nfcTeslaApp_free(NfcTeslaApp* instance) {
    furi_record_close(RECORD_STORAGE);

    view_dispatcher_remove_view(app->view_dispatcher, VIEW_DISPATCHER_MENU);
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_DISPATCHER_DEBUG);

    view_dispatcher_free(instance->view_dispatcher);
    furi_record_close(RECORD_GUI);

    furi_thread_join(instance->debug_view_thread);
    furi_thread_free(instance->debug_view_thread);

    free(instance->model);
    free(instance);
}

static bool eventCallback(void* context) {
    UNUSED(context);
    FURI_LOG_D(TAG, "Event callback");
    return false;
}

bool input_callback(InputEvent* event, void* context) {
    UNUSED(context);
    FURI_LOG_D(TAG, "Input callback");
    furi_message_queue_put(event_queue, event, FuriWaitForever);
    return true;
}

int32_t nfctesla_app() {
    furi_log_set_level(FuriLogLevelDebug);
    FURI_LOG_D(TAG, "APP STARTED");

    event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    app = nfcTeslaApp_alloc();
    Menu* mainMenu = menu_alloc();
    menu_add_item(mainMenu, "Test 1", &I_125_10px, VIEW_DISPATCHER_DEBUG, dispatch_view, app);
    menu_add_item(mainMenu, "Test 2", &I_125_10px, VIEW_DISPATCHER_DEBUG, dispatch_view, app);
    menu_add_item(mainMenu, "Test 3", &I_125_10px, VIEW_DISPATCHER_DEBUG, dispatch_view, app);

    // Debug
    textBoxDebug = text_box_alloc();
    textBoxDebugText = furi_string_alloc();
    uint32_t test = 1;
    debug_printf("Debug %lu", test);

    text_box_set_font(textBoxDebug, TextBoxFontText);
    View* textBoxDebugView = text_box_get_view(textBoxDebug);
    view_set_input_callback(textBoxDebugView, input_callback);

    // Popup
    popup = popup_alloc();
    popup_disable_timeout(popup);

    view_dispatcher_add_view(app->view_dispatcher, VIEW_DISPATCHER_MENU, menu_get_view(mainMenu));
    view_dispatcher_add_view(app->view_dispatcher, VIEW_DISPATCHER_DEBUG, textBoxDebugView);
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_DISPATCHER_MENU);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, eventCallback);

    view_dispatcher_run(app->view_dispatcher);

    nfcTeslaApp_free(app);
    menu_free(mainMenu);
    text_box_free(textBoxDebug);
    furi_string_free(textBoxDebugText);
    popup_free(popup);
    furi_message_queue_free(event_queue);

    return 0;
}