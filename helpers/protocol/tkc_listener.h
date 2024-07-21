#pragma once

typedef struct TkcListener TkcListener;

TkcListener* tkc_listener_alloc(NfcTeslaApp* app);
void tkc_listener_free(TkcListener* instance);
void tkc_listener_start(TkcListener* instance, NfcGenericCallback* callback);
void tkc_listener_stop(TkcListener* instance);
