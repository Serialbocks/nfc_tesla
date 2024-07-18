#include <furi.h>
#include <furi_hal.h>

#include "icons.h"
#include "nfc_tesla.h"

#define TAG "NFC_TESLA_main"

#define VIEW_DISPATCHER_MENU  0
#define VIEW_DISPATCHER_DEBUG 1
#define VIEW_DISPATCHER_READ  2

NfcTeslaApp* app;
Popup* popup;
FuriMessageQueue* event_queue;

void debug_printf(NfcTeslaApp* instance, const char format[], ...) {
    va_list args;
    va_start(args, format);
    furi_string_vprintf(instance->model->text_box_debug_text, format, args);
    text_box_set_text(
        instance->model->text_box_debug,
        furi_string_get_cstr(instance->model->text_box_debug_text));
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
        if(furi_status != FuriStatusOk) {
            continue;
        }
        if(input_event.type == InputTypePress && input_event.key == InputKeyBack) {
            view_dispatcher_switch_to_view(context->view_dispatcher, VIEW_DISPATCHER_MENU);
            break;
        }
    }

    return 0;
}

static void app_blink_start(NfcTeslaApp* instance) {
    notification_message(instance->notifications, &sequence_blink_start_cyan);
}

static void app_blink_stop(NfcTeslaApp* instance) {
    notification_message(instance->notifications, &sequence_blink_stop);
}

static void app_read_success(NfcTeslaApp* instance) {
    notification_message(instance->notifications, &sequence_success);
}

void scanner_callback(NfcTkcScannerEvent event, void* contextd) {
    NfcTeslaApp* context = contextd;
    if(event.type != NfcTkcScannerEventTypeNotDetected) {
        uint16_t form_factor = event.data.tkc_data.form_factor;
        uint8_t form_factor_byte_1 = ((uint8_t*)&(form_factor))[0];
        uint8_t form_factor_byte_2 = ((uint8_t*)&(form_factor))[1];
        debug_printf(
            context,
            "Card Read!\
\npublic_key byte 1: 0x%02x\
\nversion: 0x%02x%02x\
\nform_factor: 0x%02x%02x\
\nauth_challenge: 0x%02x%02x\
\nauth_challenge_result: 0x%02x%02x",
            event.data.tkc_data.public_key.data_parsed.byte_1,
            event.data.tkc_data.version_info.data_raw[0],
            event.data.tkc_data.version_info.data_raw[1],
            form_factor_byte_1,
            form_factor_byte_2,
            event.data.tkc_data.auth_challenge[4],
            event.data.tkc_data.auth_challenge[5],
            event.data.tkc_data.auth_challenge_result[4],
            event.data.tkc_data.auth_challenge_result[5]);
        app_read_success(context);
        app_blink_stop(context);
    }
    //FURI_LOG_D(TAG, "scanner_callback form_factor: %#02x", event.data.tkc_data.form_factor);
}

int32_t read_view_thread(void* contextd) {
    FuriStatus furi_status;
    InputEvent input_event;
    NfcTeslaApp* context = contextd;
    FURI_LOG_D(TAG, "read_view_thread");

    view_dispatcher_switch_to_view(context->view_dispatcher, VIEW_DISPATCHER_DEBUG);
    debug_printf(context, "Testing NFC Read...");

    app_blink_start(context);
    nfc_tkc_scanner_start(context->scanner, scanner_callback, context);
    while(true) {
        furi_status = furi_message_queue_get(event_queue, &input_event, 100);
        if(furi_status != FuriStatusOk || input_event.type != InputTypePress) {
            continue;
        }
        if(input_event.key == InputKeyBack) {
            break;
        }
    }

    nfc_tkc_scanner_stop(context->scanner);
    app_blink_stop(context);
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
    instance->scanner = nfc_tkc_scanner_alloc(instance->nfc);

    instance->view_dispatcher = view_dispatcher_alloc();

    instance->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_enable_queue(instance->view_dispatcher);
    view_dispatcher_attach_to_gui(
        instance->view_dispatcher, instance->gui, ViewDispatcherTypeFullscreen);

    instance->storage = furi_record_open(RECORD_STORAGE);

    instance->notifications = furi_record_open(RECORD_NOTIFICATION);

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

    furi_record_close(RECORD_NOTIFICATION);
    instance->notifications = NULL;

    furi_thread_join(instance->debug_view_thread);
    furi_thread_free(instance->debug_view_thread);
    furi_thread_join(instance->read_view_thread);
    furi_thread_free(instance->read_view_thread);

    nfc_tkc_scanner_free(instance->scanner);
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
    return false;
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
    app->model->text_box_debug = text_box_alloc();
    app->model->text_box_debug_text = furi_string_alloc();
    uint32_t test = 1;
    debug_printf(app, "Debug %lu", test);

    text_box_set_font(app->model->text_box_debug, TextBoxFontText);
    View* debug_view = view_alloc();
    view_set_input_callback(debug_view, input_callback);
    view_set_context(debug_view, app);

    ViewStack* debug_view_stack = view_stack_alloc();
    view_stack_add_view(debug_view_stack, debug_view);
    view_stack_add_view(debug_view_stack, text_box_get_view(app->model->text_box_debug));

    // Popup
    popup = popup_alloc();
    popup_disable_timeout(popup);

    view_dispatcher_add_view(app->view_dispatcher, VIEW_DISPATCHER_MENU, menu_get_view(mainMenu));
    view_dispatcher_add_view(
        app->view_dispatcher, VIEW_DISPATCHER_DEBUG, view_stack_get_view(debug_view_stack));
    view_dispatcher_add_view(
        app->view_dispatcher, VIEW_DISPATCHER_READ, view_stack_get_view(debug_view_stack));
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_DISPATCHER_MENU);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, eventCallback);

    view_dispatcher_run(app->view_dispatcher);

    view_stack_free(debug_view_stack);
    text_box_free(app->model->text_box_debug);
    furi_string_free(app->model->text_box_debug_text);
    nfcTeslaApp_free(app);
    menu_free(mainMenu);

    popup_free(popup);
    furi_message_queue_free(event_queue);

    return 0;
}
