#pragma once

typedef struct TkcListener TkcListener;

TkcListener* tkc_listener_alloc();
void tkc_listener_free(TkcListener* instance);
void tkc_listener_start(TkcListener* instance);
void tkc_listener_stop(TkcListener* instance);
