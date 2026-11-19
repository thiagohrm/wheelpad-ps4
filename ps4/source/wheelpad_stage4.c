/*
 * Stage 4 probe: the real plugin logic (pump_socket + merge_wheel) but without
 * any UDP socket. This tests the parsing/mapping path in isolation.
 */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <orbis/Pad.h>
#include <orbis/_types/pad.h>

#include <Common.h>

#include "mapping.h"
#include "report.h"
#include "wire.h"

#define attr_module_hidden __attribute__((weak)) __attribute__((visibility("hidden")))
#define attr_public __attribute__((visibility("default")))

attr_public const char *g_pluginName = "wheelpad";
attr_public const char *g_pluginDesc = "stage 4 probe";
attr_public const char *g_pluginAuth = "volante";
attr_public uint32_t g_pluginVersion = 0x00000105; /* 1.05, stage 4 */

HOOK_INIT(scePadRead);
HOOK_INIT(scePadReadState);

static uint8_t g_frame[VOLANTE_REPORT_LEN];
static int g_have_frame = 0;
static int g_last_packet = 0;
static int g_logged_live = 0;

static int64_t now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (int64_t)tv.tv_usec / 1000;
}

static void merge_wheel(OrbisPadData *data)
{
    volante_report wheel;
    volante_pad_state pad;

    if (!g_have_frame || data == NULL) {
        return;
    }

    if (volante_parse(g_frame, VOLANTE_REPORT_LEN, &wheel) != 0) {
        return;
    }

    volante_map(&wheel, &pad);

    if (!g_logged_live) {
        printf("stage4: live steering=%u\n", pad.left_x);
        fflush(stdout);
        g_logged_live = 1;
    }

    data->leftStick.x = pad.left_x;
    data->analogButtons.l2 = pad.l2;
    data->analogButtons.r2 = pad.r2;
    data->buttons |= pad.buttons;
}

int32_t scePadRead_hook(int32_t handle, OrbisPadData *data, int32_t count)
{
    int32_t result;
    int32_t index;

    result = HOOK_CONTINUE(scePadRead, int32_t (*)(int32_t, OrbisPadData *, int32_t), handle, data, count);

    if (result > 0 && data != NULL) {
        for (index = 0; index < result && index < count; index++) {
            merge_wheel(&data[index]);
        }
    }
    return result;
}

int32_t scePadReadState_hook(int32_t handle, OrbisPadData *data)
{
    int32_t result;

    result = HOOK_CONTINUE(scePadReadState, int32_t (*)(int32_t, OrbisPadData *), handle, data);

    if (result == 0) {
        merge_wheel(data);
    }
    return result;
}

int32_t attr_public plugin_load(int32_t argc, const char *argv[])
{
    (void)argc;
    (void)argv;

    printf("stage4: enter\n");
    fflush(stdout);

    Detour_Construct(&Detour_scePadRead, DetourMode_x32);
    Detour_DetourFunction(&Detour_scePadRead, (uint64_t)scePadRead, (void *)scePadRead_hook);

    Detour_Construct(&Detour_scePadReadState, DetourMode_x32);
    Detour_DetourFunction(&Detour_scePadReadState, (uint64_t)scePadReadState, (void *)scePadReadState_hook);

    printf("stage4: hooks installed\n");
    fflush(stdout);

    /* Synthetic report so merge_wheel has something to do even without the bridge. */
    memset(g_frame, 0, VOLANTE_REPORT_LEN);
    g_frame[1] = 0xE0;
    g_frame[2] = 0x0F;
    g_frame[3] = 0x40;
    g_frame[4] = 0x80;
    g_frame[5] = 0x80;
    g_frame[6] = 0x80;
    g_have_frame = 1;
    g_last_packet = now_ms();

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
