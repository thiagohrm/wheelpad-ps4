/*
 * Minimal loadability probe with the same build pipeline as wheelpad.c.
 *
 * plugin_load does nothing and the module imports no library function at
 * all. If a title still crashes with this loaded, the fault is in the PRX
 * structure (link or create-fself), not in the plugin code. If it survives,
 * the fault is in what the real plugin calls, and the next probe adds calls
 * back one at a time.
 */

#include <stdint.h>

#define attr_module_hidden __attribute__((weak)) __attribute__((visibility("hidden")))
#define attr_public __attribute__((visibility("default")))

attr_public const char *g_pluginName = "wheelpad";
attr_public const char *g_pluginDesc = "loadability probe";
attr_public const char *g_pluginAuth = "volante";
attr_public uint32_t g_pluginVersion = 0x00000101; /* 1.01, probe */

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
