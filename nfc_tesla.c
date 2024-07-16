#include <furi.h>
#include <furi_hal.h>

#include "icons.h"
#include "nfc_tesla.h"
#include "helpers/nfc_tkc_scanner.h"

#define TAG "NFC_TESLA_main"

#define VIEW_DISPATCHER_MENU 0
#define VIEW_DISPATCHER_DEBUG 1
#define VIEW_DISPATCHER_READ 2

NfcTeslaApp* app;
Popup* popup;
FuriMessageQueue* event_queue;

void debug_printf(NfcTeslaApp* instance, const char format[], ...) {
    va_list args;
    va_start(args, format);
    furi_string_vprintf(instance->model->textBoxDebugText, format, args);
    text_box_set_text(
        instance->model->textBoxDebug, furi_string_get_cstr(instance->model->textBoxDebugText));
    va_end(args);
}

int32_t debug_view_thread(void* contextd) {
    NfcTeslaApp* context = contextd;
    InputEvent input_event;
    FuriStatus furi_status;

    FURI_LOG_D(TAG, "debug_view_thread");
    view_dispatcher_switch_to_view(context->view_dispatcher, VIEW_DISPATCHER_DEBUG);

    while(true) {
        furi_status = furi_message_queue_get(event_queue, &input_event, 100);
        if(furi_status != FuriStatusOk || input_event.type != InputTypePress) {
            continue;
        }
        if(input_event.key == InputKeyBack) {
            view_dispatcher_switch_to_view(context->view_dispatcher, VIEW_DISPATCHER_MENU);
            break;
        }
    }

    return 0;
}

int32_t read_view_thread(void* contextd) {
    NfcTeslaApp* context = contextd;
    FURI_LOG_D(TAG, "read_view_thread");
    view_dispatcher_switch_to_view(context->view_dispatcher, VIEW_DISPATCHER_DEBUG);
    debug_printf(context, "Testing NFC Read...");

    furi_delay_ms(2000);
    debug_printf(context, "Done! Stopping...");

    furi_delay_ms(2000);
    view_dispatcher_switch_to_view(context->view_dispatcher, VIEW_DISPATCHER_MENU);
    return 0;
}

static void dispatch_view(void* contextd, uint32_t index) {
    NfcTeslaApp* context = (NfcTeslaApp*)contextd;

    if(index == VIEW_DISPATCHER_DEBUG) {
        furi_thread_start(context->debug_view_thread);
    } else if(index == VIEW_DISPATCHER_READ) {
        furi_thread_start(context->read_view_thread);
    }
}

static NfcTeslaApp* nfcTeslaApp_alloc() {
    FURI_LOG_D(TAG, "alloc");
    NfcTeslaApp* instance = malloc(sizeof(NfcTeslaApp));

    instance->model = malloc(sizeof(NfcTeslaAppModel));
    memset(instance->model, 0x0, sizeof(NfcTeslaAppModel));

    instance->nfc = nfc_alloc();
    instance->source_dev = nfc_device_alloc();
    instance->target_dev = nfc_device_alloc();

    instance->view_dispatcher = view_dispatcher_alloc();

    instance->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_enable_queue(instance->view_dispatcher);
    view_dispatcher_attach_to_gui(
        instance->view_dispatcher, instance->gui, ViewDispatcherTypeFullscreen);

    instance->storage = furi_record_open(RECORD_STORAGE);

    instance->debug_view_thread =
        furi_thread_alloc_ex("debug_view_thread", 512, debug_view_thread, instance);
    instance->read_view_thread =
        furi_thread_alloc_ex("read_view_thread", 512, read_view_thread, instance);

    return instance;
}

static void nfcTeslaApp_free(NfcTeslaApp* instance) {
    furi_record_close(RECORD_STORAGE);

    view_dispatcher_remove_view(app->view_dispatcher, VIEW_DISPATCHER_MENU);
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_DISPATCHER_DEBUG);
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_DISPATCHER_READ);

    view_dispatcher_free(instance->view_dispatcher);
    furi_record_close(RECORD_GUI);

    furi_thread_join(instance->debug_view_thread);
    furi_thread_free(instance->debug_view_thread);
    furi_thread_join(instance->read_view_thread);
    furi_thread_free(instance->read_view_thread);

    nfc_free(instance->nfc);
    nfc_device_free(instance->source_dev);
    nfc_device_free(instance->target_dev);

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
    menu_add_item(mainMenu, "Test Read", &I_125_10px, VIEW_DISPATCHER_READ, dispatch_view, app);
    menu_add_item(mainMenu, "Test 3", &I_125_10px, VIEW_DISPATCHER_DEBUG, dispatch_view, app);

    // Debug
    app->model->textBoxDebug = text_box_alloc();
    app->model->textBoxDebugText = furi_string_alloc();
    uint32_t test = 1;
    debug_printf(app, "Debug %lu", test);

    text_box_set_font(app->model->textBoxDebug, TextBoxFontText);
    View* textBoxDebugView = text_box_get_view(app->model->textBoxDebug);
    view_set_input_callback(textBoxDebugView, input_callback);

    // Popup
    popup = popup_alloc();
    popup_disable_timeout(popup);

    view_dispatcher_add_view(app->view_dispatcher, VIEW_DISPATCHER_MENU, menu_get_view(mainMenu));
    view_dispatcher_add_view(app->view_dispatcher, VIEW_DISPATCHER_DEBUG, textBoxDebugView);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_DISPATCHER_READ, textBoxDebugView);
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_DISPATCHER_MENU);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, eventCallback);

    view_dispatcher_run(app->view_dispatcher);

    FURI_LOG_D(TAG, "text_box_free");
    text_box_free(app->model->textBoxDebug);
    furi_string_free(app->model->textBoxDebugText);
    FURI_LOG_D(TAG, "nfcTeslaApp_free");
    nfcTeslaApp_free(app);
    FURI_LOG_D(TAG, "menu_free");
    menu_free(mainMenu);

    popup_free(popup);
    furi_message_queue_free(event_queue);

    return 0;
}