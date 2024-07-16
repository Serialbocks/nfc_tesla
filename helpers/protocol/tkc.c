#include "tkc.h"
#include "core/check.h"

Tkc* tkc_alloc() {
    Tkc* instance = malloc(sizeof(Tkc));

    return instance;
}

void tkc_free(Tkc* instance) {
    furi_check(instance);

    free(instance);
}

void tkc_reset(Tkc* instance) {
    furi_check(instance);

    memset(&instance->config, 0, sizeof(TkcConfig));
}

void tkc_copy(Tkc* dest, const Tkc* source) {
    furi_check(dest);
    furi_check(source);

    memcpy(dest, source, sizeof(Tkc));
}