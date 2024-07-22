#include <furi.h>
#include <furi_hal.h>

#include "icons.h"
#include "nfc_tesla.h"

#define TAG "NFC_TESLA_main"

#define VIEW_DISPATCHER_MENU   0
#define VIEW_DISPATCHER_READ   1
#define VIEW_DISPATCHER_LISTEN 2

NfcTeslaApp* app;
Popup* popup;
FuriMessageQueue* event_queue;

static void
    textbox_printf(TextBox* text_box, FuriString* text_box_text, const char format[], ...) {
    va_list args;
    va_start(args, format);
    furi_string_vprintf(text_box_text, format, args);
    text_box_set_text(text_box, furi_string_get_cstr(text_box_text));
    va_end(args);
}

static void
    textbox_cat_printf(TextBox* text_box, FuriString* text_box_text, const char format[], ...) {
    va_list args;
    va_start(args, format);
    furi_string_cat_printf(text_box_text, format, args);
    text_box_set_text(text_box, furi_string_get_cstr(text_box_text));
    va_end(args);
}

static void textbox_cat_print_bytes(
    TextBox* text_box,
    FuriString* text_box_text,
    uint8_t* data,
    uint8_t length) {
    furi_string_cat_printf(text_box_text, "0x");
    for(uint8_t i = 0; i < length; i++) {
        furi_string_cat_printf(text_box_text, "%02x", data[i]);
    }

    text_box_set_text(text_box, furi_string_get_cstr(text_box_text));
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

static bool load_file(NfcTeslaApp* instance, FuriString* path, bool show_dialog) {
    furi_assert(instance);
    furi_assert(path);

    FURI_LOG_D(TAG, "nfc_device_load");
    FURI_LOG_D(TAG, furi_string_get_cstr(path));
    bool result = nfc_device_load(instance->nfc_device, furi_string_get_cstr(path));

    if(result) {
        FURI_LOG_D(TAG, "path_extract_filename");
        path_extract_filename(path, instance->file_name, true);
    }

    if((!result) && (show_dialog)) {
        FURI_LOG_D(TAG, "dialog_message_show_storage_error");
        dialog_message_show_storage_error(instance->dialogs, "Cannot load\nkey file");
    }

    FURI_LOG_D(TAG, "load_file return result");
    return result;
}

static bool load_from_file_select(NfcTeslaApp* instance) {
    furi_assert(instance);

    FURI_LOG_D(TAG, "load_from_file_select");
    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, NFC_TESLA_APP_EXTENSION, &I_125_10px);
    browser_options.base_path = NFC_TESLA_APP_FOLDER;
    browser_options.hide_dot_files = true;

    bool success = false;
    do {
        // Input events and views are managed by file_browser
        FURI_LOG_D(TAG, "dialog_file_browser_show");
        if(!dialog_file_browser_show(
               instance->dialogs, instance->file_path, instance->file_path, &browser_options))
            break;
        FURI_LOG_D(TAG, "load_file");
        success = load_file(instance, instance->file_path, true);
    } while(!success);

    return success;
}

void scanner_callback(NfcTkcScannerEvent event, void* contextd) {
    NfcTeslaApp* context = contextd;
    if(event.type != NfcTkcScannerEventTypeNotDetected) {
        uint16_t form_factor = event.data.tkc_data->form_factor;
        uint8_t form_factor_byte_1 = ((uint8_t*)&(form_factor))[0];
        uint8_t form_factor_byte_2 = ((uint8_t*)&(form_factor))[1];

        textbox_printf(
            context->model->text_box_read,
            context->model->text_box_read_text,
            "Read success!\
\nversion: 0x%02x%02x\
\nform_factor: 0x%02x%02x\
\nauth_challenge_res: %d\
\nsak: 0x%02x\
\natqa: 0x%02x%02x\
\nuid: ",
            event.data.tkc_data->version_info.data_raw[0],
            event.data.tkc_data->version_info.data_raw[1],
            form_factor_byte_1,
            form_factor_byte_2,
            event.data.tkc_data->auth_challenge_is_successful,
            event.data.tkc_data->iso14443_4a_data->iso14443_3a_data->sak,
            event.data.tkc_data->iso14443_4a_data->iso14443_3a_data->atqa[0],
            event.data.tkc_data->iso14443_4a_data->iso14443_3a_data->atqa[1]);
        textbox_cat_print_bytes(
            context->model->text_box_read,
            context->model->text_box_read_text,
            event.data.tkc_data->iso14443_4a_data->iso14443_3a_data->uid,
            event.data.tkc_data->iso14443_4a_data->iso14443_3a_data->uid_len);

        textbox_cat_printf(
            context->model->text_box_read, context->model->text_box_read_text, "\nATS: ");
        textbox_cat_print_bytes(
            context->model->text_box_read,
            context->model->text_box_read_text,
            (uint8_t*)&event.data.tkc_data->iso14443_4a_data->ats_data,
            5);
        textbox_cat_printf(
            context->model->text_box_read,
            context->model->text_box_read_text,
            "\nFSCI elem count: %d",
            simple_array_get_count(event.data.tkc_data->iso14443_4a_data->ats_data.t1_tk));

        textbox_cat_printf(
            context->model->text_box_read, context->model->text_box_read_text, "\nPublic key: ");
        textbox_cat_print_bytes(
            context->model->text_box_read,
            context->model->text_box_read_text,
            event.data.tkc_data->public_key.data_parsed.public_key,
            TKC_PUBLIC_KEY_SIZE - 1);

        app_read_success(context);
        app_blink_stop(context);
    }
}

int32_t read_view_thread(void* contextd) {
    FuriStatus furi_status;
    InputEvent input_event;
    NfcTeslaApp* context = contextd;
    FURI_LOG_D(TAG, "read_view_thread");

    view_dispatcher_switch_to_view(context->view_dispatcher, VIEW_DISPATCHER_READ);
    textbox_printf(
        context->model->text_box_read, context->model->text_box_read_text, "Reading card...");

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

static void listener_callback(NfcTkcListenerEvent event, void* contextd) {
    NfcTkcListener* instance = contextd;
    UNUSED(event);
    UNUSED(instance);
}

int32_t listen_view_thread(void* contextd) {
    FuriStatus furi_status;
    InputEvent input_event;
    NfcTeslaApp* context = contextd;

    bool success = load_from_file_select(context);
    if(success) {
        FURI_LOG_D(TAG, "view_dispatcher_switch_to_view VIEW_DISPATCHER_MENU");
        view_dispatcher_switch_to_view(context->view_dispatcher, VIEW_DISPATCHER_MENU);
        return 0;
    }

    FURI_LOG_D(TAG, "listen_view_thread");
    textbox_printf(
        context->model->text_box_listen,
        context->model->text_box_listen_text,
        "Emulating Key Card...");

    FURI_LOG_D(TAG, "app_blink_start");

    app_blink_start(context);
    FURI_LOG_D(TAG, "nfc_tkc_listener_start");
    nfc_tkc_listener_start(context->listener, listener_callback);
    while(true) {
        FURI_LOG_D(TAG, "furi_message_queue_get");
        furi_status = furi_message_queue_get(event_queue, &input_event, 100);
        if(furi_status != FuriStatusOk || input_event.type != InputTypePress) {
            continue;
        }
        if(input_event.key == InputKeyBack) {
            break;
        }
    }

    FURI_LOG_D(TAG, "nfc_tkc_listener_stop");
    nfc_tkc_listener_stop(context->listener);
    FURI_LOG_D(TAG, "app_blink_stop");
    app_blink_stop(context);
    FURI_LOG_D(TAG, "view_dispatcher_switch_to_view");
    view_dispatcher_switch_to_view(context->view_dispatcher, VIEW_DISPATCHER_MENU);
    return 0;
}

static void dispatch_view(void* contextd, uint32_t index) {
    NfcTeslaApp* context = (NfcTeslaApp*)contextd;

    if(index == VIEW_DISPATCHER_READ) {
        furi_thread_start(context->read_view_thread);
    } else if(index == VIEW_DISPATCHER_LISTEN) {
        FURI_LOG_D(TAG, "furi_thread_start listen_view_thread");
        furi_thread_start(context->listen_view_thread);
    }
}

static NfcTeslaApp* nfcTeslaApp_alloc() {
    FURI_LOG_D(TAG, "alloc");
    NfcTeslaApp* instance = malloc(sizeof(NfcTeslaApp));

    FURI_LOG_D(TAG, "malloc");
    instance->model = malloc(sizeof(NfcTeslaAppModel));
    FURI_LOG_D(TAG, "memset");
    memset(instance->model, 0x0, sizeof(NfcTeslaAppModel));

    FURI_LOG_D(TAG, "nfc_alloc");
    instance->nfc = nfc_alloc();
    FURI_LOG_D(TAG, "nfc_tkc_scanner_alloc");
    instance->scanner = nfc_tkc_scanner_alloc(instance->nfc);
    FURI_LOG_D(TAG, "nfc_device_alloc");
    instance->nfc_device = nfc_device_alloc();
    FURI_LOG_D(TAG, "nfc_tkc_listener_alloc");
    instance->listener = nfc_tkc_listener_alloc(instance);

    FURI_LOG_D(TAG, "view_dispatcher_alloc");
    instance->view_dispatcher = view_dispatcher_alloc();

    instance->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_enable_queue(instance->view_dispatcher);
    view_dispatcher_attach_to_gui(
        instance->view_dispatcher, instance->gui, ViewDispatcherTypeFullscreen);

    instance->storage = furi_record_open(RECORD_STORAGE);
    instance->dialogs = furi_record_open(RECORD_DIALOGS);
    instance->notifications = furi_record_open(RECORD_NOTIFICATION);

    instance->file_path = furi_string_alloc_set(NFC_TESLA_APP_FOLDER);
    instance->file_name = furi_string_alloc();

    instance->read_view_thread =
        furi_thread_alloc_ex("read_view_thread", 512, read_view_thread, instance);
    instance->listen_view_thread =
        furi_thread_alloc_ex("listen_view_thread", 512 * 2, listen_view_thread, instance);

    return instance;
}

static void nfcTeslaApp_free(NfcTeslaApp* instance) {
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);

    furi_string_free(instance->file_path);
    furi_string_free(instance->file_name);

    view_dispatcher_remove_view(app->view_dispatcher, VIEW_DISPATCHER_MENU);
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_DISPATCHER_READ);
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_DISPATCHER_LISTEN);

    view_dispatcher_free(instance->view_dispatcher);
    furi_record_close(RECORD_GUI);

    furi_record_close(RECORD_NOTIFICATION);
    instance->notifications = NULL;

    furi_thread_join(instance->read_view_thread);
    furi_thread_free(instance->read_view_thread);
    furi_thread_join(instance->listen_view_thread);
    furi_thread_free(instance->listen_view_thread);

    nfc_tkc_listener_free(instance->listener);
    nfc_tkc_scanner_free(instance->scanner);
    nfc_device_free(instance->nfc_device);
    nfc_free(instance->nfc);

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
    menu_add_item(mainMenu, "Read Card", &I_125_10px, VIEW_DISPATCHER_READ, dispatch_view, app);
    menu_add_item(
        mainMenu, "Load Saved Card", &I_125_10px, VIEW_DISPATCHER_LISTEN, dispatch_view, app);

    // Debug
    app->model->text_box_read = text_box_alloc();
    app->model->text_box_read_text = furi_string_alloc();

    text_box_set_font(app->model->text_box_read, TextBoxFontText);
    View* read_view = view_alloc();
    view_set_input_callback(read_view, input_callback);
    view_set_context(read_view, app);

    ViewStack* read_view_stack = view_stack_alloc();
    view_stack_add_view(read_view_stack, read_view);
    view_stack_add_view(read_view_stack, text_box_get_view(app->model->text_box_read));

    app->model->text_box_listen = text_box_alloc();
    app->model->text_box_listen_text = furi_string_alloc();

    text_box_set_font(app->model->text_box_listen, TextBoxFontText);
    View* listen_view = view_alloc();
    view_set_input_callback(listen_view, input_callback);
    view_set_context(listen_view, app);

    ViewStack* listen_view_stack = view_stack_alloc();
    view_stack_add_view(listen_view_stack, listen_view);
    view_stack_add_view(listen_view_stack, text_box_get_view(app->model->text_box_listen));

    // Popup
    popup = popup_alloc();
    popup_disable_timeout(popup);

    view_dispatcher_add_view(app->view_dispatcher, VIEW_DISPATCHER_MENU, menu_get_view(mainMenu));
    view_dispatcher_add_view(
        app->view_dispatcher, VIEW_DISPATCHER_READ, view_stack_get_view(read_view_stack));
    view_dispatcher_add_view(
        app->view_dispatcher, VIEW_DISPATCHER_LISTEN, view_stack_get_view(listen_view_stack));
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_DISPATCHER_MENU);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, eventCallback);

    view_dispatcher_run(app->view_dispatcher);

    view_stack_free(read_view_stack);
    text_box_free(app->model->text_box_read);
    furi_string_free(app->model->text_box_read_text);

    view_stack_free(listen_view_stack);
    text_box_free(app->model->text_box_listen);
    furi_string_free(app->model->text_box_listen_text);

    nfcTeslaApp_free(app);
    menu_free(mainMenu);

    popup_free(popup);
    furi_message_queue_free(event_queue);

    return 0;
}
