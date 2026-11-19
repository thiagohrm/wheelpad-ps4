/*
 * Discovery probe: hook every plausible pad read entry point at once and
 * log which one the game actually calls.
 *
 * The game may use the public wrapper, the internal *Ext function, the
 * history variant, or the shared-output path. Hooking them all in one run
 * identifies the real one in a single launch instead of one launch per
 * candidate.
 *
 * Only the first call of each is logged, to keep klog readable.
 */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <orbis/Pad.h>
#include <orbis/_types/pad.h>
#include <orbis/libkernel.h>

#include <Common.h>

#define attr_module_hidden __attribute__((weak)) __attribute__((visibility("hidden")))
#define attr_public __attribute__((visibility("default")))

attr_public const char *g_pluginName = "wheelpad";
attr_public const char *g_pluginDesc = "pad entry point discovery";
attr_public const char *g_pluginAuth = "volante";
attr_public uint32_t g_pluginVersion = 0x00000400; /* 4.00, discovery */

#define STEP(...)            \
    do {                     \
        printf(__VA_ARGS__); \
        fflush(stdout);      \
    } while (0)

HOOK_INIT(scePadRead);
HOOK_INIT(scePadReadState);
HOOK_INIT(scePadReadExt);
HOOK_INIT(scePadReadStateExt);
HOOK_INIT(scePadReadHistory);
HOOK_INIT(scePadReadForTracker);
HOOK_INIT(scePadShareOutputData);
HOOK_INIT(scePadGetDataInternal);

static int g_seen[8];

static void report(int index, const char *name)
{
    if (g_seen[index]) {
        return;
    }
    g_seen[index] = 1;
    STEP("[wheelpad] CALLED %s\n", name);
}

int32_t scePadRead_hook(int32_t handle, OrbisPadData *data, int32_t count)
{
    report(0, "scePadRead");
    return HOOK_CONTINUE(scePadRead, int32_t (*)(int32_t, OrbisPadData *, int32_t), handle, data, count);
}

int32_t scePadReadState_hook(int32_t handle, OrbisPadData *data)
{
    report(1, "scePadReadState");
    return HOOK_CONTINUE(scePadReadState, int32_t (*)(int32_t, OrbisPadData *), handle, data);
}

int32_t scePadReadExt_hook(int32_t handle, OrbisPadData *data, int32_t count)
{
    report(2, "scePadReadExt");
    return HOOK_CONTINUE(scePadReadExt, int32_t (*)(int32_t, OrbisPadData *, int32_t), handle, data, count);
}

int32_t scePadReadStateExt_hook(int32_t handle, OrbisPadData *data)
{
    report(3, "scePadReadStateExt");
    return HOOK_CONTINUE(scePadReadStateExt, int32_t (*)(int32_t, OrbisPadData *), handle, data);
}

int32_t scePadReadHistory_hook(int32_t handle, OrbisPadData *data, int32_t count)
{
    report(4, "scePadReadHistory");
    return HOOK_CONTINUE(scePadReadHistory, int32_t (*)(int32_t, OrbisPadData *, int32_t), handle, data, count);
}

int32_t scePadReadForTracker_hook(int32_t handle, OrbisPadData *data, int32_t count)
{
    report(5, "scePadReadForTracker");
    return HOOK_CONTINUE(scePadReadForTracker, int32_t (*)(int32_t, OrbisPadData *, int32_t), handle, data, count);
}

int32_t scePadShareOutputData_hook(int32_t handle, OrbisPadData *data, int32_t count)
{
    report(6, "scePadShareOutputData");
    return HOOK_CONTINUE(scePadShareOutputData, int32_t (*)(int32_t, OrbisPadData *, int32_t), handle, data, count);
}

int32_t scePadGetDataInternal_hook(int32_t handle, OrbisPadData *data, int32_t count)
{
    report(7, "scePadGetDataInternal");
    return HOOK_CONTINUE(scePadGetDataInternal, int32_t (*)(int32_t, OrbisPadData *, int32_t), handle, data, count);
}

/* The Detour target has to be the real function inside libScePad, so the
 * symbols are resolved with dlsym rather than taken from our own imports. */
static uint64_t resolve(const char *name)
{
    char path[256];
    int handle = 0;
    uint64_t address = 0;

    snprintf(path, sizeof(path), "/%s/common/lib/libScePad.sprx",
             sceKernelGetFsSandboxRandomWord());

    if (sys_dynlib_load_prx(path, &handle) == 0 && handle != 0) {
        if (sys_dynlib_dlsym(handle, name, &address) != 0) {
            address = 0;
        }
    }
    return address;
}

static void hook_one(const char *name, Detour *detour, void *hook, uint64_t fallback)
{
    uint64_t address = resolve(name);

    if (address == 0) {
        STEP("[wheelpad] resolve failed for %s\n", name);
        return;
    }

    Detour_Construct(detour, DetourMode_x32);
    Detour_DetourFunction(detour, address, hook);
    STEP("[wheelpad] hooked %s at 0x%lx\n", name, (long)address);
}

int32_t attr_public plugin_load(int32_t argc, const char *argv[])
{
    (void)argc;
    (void)argv;

    STEP("[wheelpad] discovery start\n");

    hook_one("scePadRead", &Detour_scePadRead, (void *)scePadRead_hook, (uint64_t)scePadRead);
    hook_one("scePadReadState", &Detour_scePadReadState, (void *)scePadReadState_hook, (uint64_t)scePadReadState);
    hook_one("scePadReadExt", &Detour_scePadReadExt, (void *)scePadReadExt_hook, (uint64_t)scePadReadExt);
    hook_one("scePadReadStateExt", &Detour_scePadReadStateExt, (void *)scePadReadStateExt_hook, (uint64_t)scePadReadStateExt);
    hook_one("scePadReadHistory", &Detour_scePadReadHistory, (void *)scePadReadHistory_hook, (uint64_t)scePadReadHistory);
    hook_one("scePadReadForTracker", &Detour_scePadReadForTracker, (void *)scePadReadForTracker_hook, (uint64_t)scePadReadForTracker);
    hook_one("scePadShareOutputData", &Detour_scePadShareOutputData, (void *)scePadShareOutputData_hook, (uint64_t)scePadShareOutputData);
    hook_one("scePadGetDataInternal", &Detour_scePadGetDataInternal, (void *)scePadGetDataInternal_hook, (uint64_t)scePadGetDataInternal);

    STEP("[wheelpad] discovery done\n");
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
