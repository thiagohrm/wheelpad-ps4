/*
 * Stage 1 probe: minimal PRX plus two libc calls at load (fopen and printf).
 *
 * If this crashes, the fault is in calling libc through our PRX's import
 * path, not in the plugin logic. If it survives, the fault is in socket or
 * the Detour hooks.
 */

#include <stdint.h>
#include <stdio.h>

#define attr_module_hidden __attribute__((weak)) __attribute__((visibility("hidden")))
#define attr_public __attribute__((visibility("default")))

attr_public const char *g_pluginName = "wheelpad";
attr_public const char *g_pluginDesc = "stage 1 probe";
attr_public const char *g_pluginAuth = "volante";
attr_public uint32_t g_pluginVersion = 0x00000102; /* 1.02, stage 1 */

static void logline(const char *text)
{
    FILE *file = fopen("/data/GoldHEN/wheelpad.log", "a");
    if (file == NULL) {
        file = fopen("/user/data/wheelpad.log", "a");
    }
    if (file == NULL) {
        return;
    }
    fputs(text, file);
    fputc('\n', file);
    fclose(file);
}

int32_t attr_public plugin_load(int32_t argc, const char *argv[])
{
    (void)argc;
    (void)argv;

    logline("stage1: enter");
    printf("stage1: enter\n");
    fflush(stdout);
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
