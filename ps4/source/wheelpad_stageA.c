/*
 * Stage A probe: plugin_load calls nothing at all. Establishes that reaching
 * the body is safe.
 */

#include <stdint.h>

#define attr_module_hidden __attribute__((weak)) __attribute__((visibility("hidden")))
#define attr_public __attribute__((visibility("default")))

attr_public const char *g_pluginName = "wheelpad";
attr_public const char *g_pluginDesc = "stage A probe";
attr_public const char *g_pluginAuth = "volante";
attr_public uint32_t g_pluginVersion = 0x00000200; /* 2.00, stage A */

int32_t attr_public plugin_load(int32_t argc, const char *argv[])
{
    (void)argc;
    (void)argv;
    return 0;
}

int32_t attr_public plugin_unload(int32_t argc, const char *argv[])
{
    (void)argc;
    (void)argv;
    return 0;
}

int32_t attr_module_hidden module_start(int64_t argc, const void *args)
{
    (void)argc;
    (void)args;
    return 0;
}

int32_t attr_module_hidden module_stop(int64_t argc, const void *args)
{
    (void)argc;
    (void)args;
    return 0;
}
