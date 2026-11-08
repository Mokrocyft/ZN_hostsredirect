#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZYGISK_NEXT_API_VERSION_1 3

struct ZnSymbolResolver;
struct ZygiskNextRuntime;

struct ZygiskNextAPI {
    int (*pltHook)(void* base, const char* sym, void* handler, void** orig);
    int (*inlineHook)(void* target, void* addr, void** orig);
    int (*inlineUnhook)(void* target);
    struct ZnSymbolResolver* (*newSymbolResolver)(const char* path, void* base);
    void (*freeSymbolResolver)(struct ZnSymbolResolver* res);
    void* (*getBaseAddress)(struct ZnSymbolResolver* res);
    void* (*symbolLookup)(struct ZnSymbolResolver* res, const char* name, bool prefix, size_t* size);
    void (*forEachSymbols)(struct ZnSymbolResolver* res,
                           bool (*cb)(const char* name, void* addr, size_t sz, void* data), void* data);
    int (*connectCompanion)(void* handle);
    const struct ZygiskNextRuntime* (*getRuntime)(void);
};

struct ZygiskNextModule {
    int target_api_version;
    void (*onModuleLoaded)(void* self_handle, const struct ZygiskNextAPI* api);
};

struct ZygiskNextCompanionModule {
    int target_api_version;
    void (*onCompanionLoaded)(void);
    void (*onModuleConnected)(int fd);
};

#ifdef __cplusplus
}
#endif
