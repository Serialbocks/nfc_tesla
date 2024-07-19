#include "tkc.h"
#include "core/check.h"

Tkc* tkc_alloc() {
    Tkc* instance = malloc(sizeof(Tkc));
    instance->iso14443_4a_data = iso14443_4a_alloc();

    return instance;
}

void tkc_free(Tkc* instance) {
    furi_check(instance);

    iso14443_4a_free(instance->iso14443_4a_data);
    free(instance);
}

void tkc_reset(Tkc* instance) {
    furi_check(instance);

    memset(instance, 0, sizeof(Tkc) - sizeof(instance->iso14443_4a_data));
    iso14443_4a_reset(instance->iso14443_4a_data);
}

void tkc_copy(Tkc* dest, const Tkc* source) {
    furi_check(dest);
    furi_check(source);

    memcpy(dest, source, sizeof(Tkc) - sizeof(SimpleArray*));
    iso14443_4a_copy(dest->iso14443_4a_data, source->iso14443_4a_data);
}