/*
 * Stage 5 probe: the full plugin (pump_socket + merge_wheel) but with the
 * socket bound at load, as in the real plugin. This is the last piece.
 */

#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <orbis/Pad.h>
#include <orbis/_types/pad.h>

#include <Common.h>

#include "mapping.h"
#include "report.h"
#include "wire.h"

#define attr_module_hidden __attribute__((weak)) __attribute__((visibility("hidden")))
#define attr_public __attribute__((visibility("default")))

attr_public const char *g_pluginName = "wheelpad";
attr_public const char *g_pluginDesc = "stage 5 probe";
attr_public const char *g_pluginAuth = "volante";
attr_public uint32_t g_pluginVersion = 0x00000106; /* 1.06, stage 5 */

HOOK_INIT(scePadRead);
HOOK_INIT(scePadReadState);

static int g_sock = -1;
static int64_t g_next_socket_try = 0;
static int64_t g_last_packet = 0;
static uint8_t g_frame[VOLANTE_REPORT_LEN];
static int g_have_frame = 0;
static int g_logged_live = 0;

static int64_t now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (int64_t)tv.tv_usec / 1000;
}

static int open_socket(void)
{
    struct sockaddr_in addr;
    int flags;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0) {
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(9107);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(sock);
        return -1;
    }

    flags = fcntl(sock, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }
    return sock;
}

static void pump_socket(void)
{
    uint8_t buffer[64];
    int index;

    if (g_sock < 0) {
        int64_t now = now_ms();

        if (now < g_next_socket_try) {
            return;
        }
        g_sock = open_socket();
        if (g_sock < 0) {
            g_next_socket_try = now + 1000;
            return;
        }
        printf("stage5: listening on udp/%d\n", 9107);
        fflush(stdout);
    }

    for (index = 0; index < 8; index++) {
        int len = recvfrom(g_sock, buffer, sizeof(buffer), 0, NULL, NULL);

        if (len <= 0) {
            break;
        }
        if (!volante_wire_valid(buffer, (unsigned)len)) {
            continue;
        }
        memcpy(g_frame, buffer + VOLANTE_WIRE_HEADER, VOLANTE_REPORT_LEN);
        g_have_frame = 1;
        g_last_packet = now_ms();
        printf("stage5: received a report\n");
        fflush(stdout);
    }
}

static void merge_wheel(OrbisPadData *data)
{
    volante_report wheel;
    volante_pad_state pad;

    if (!g_have_frame || data == NULL) {
        return;
    }
    if (now_ms() - g_last_packet > 300) {
        return;
    }
    if (volante_parse(g_frame, VOLANTE_REPORT_LEN, &wheel) != 0) {
        return;
    }

    volante_map(&wheel, &pad);

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

    pump_socket();
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

    pump_socket();
    if (result == 0) {
        merge_wheel(data);
    }
    return result;
}

int32_t attr_public plugin_load(int32_t argc, const char *argv[])
{
    (void)argc;
    (void)argv;

    printf("stage5: enter\n");
    fflush(stdout);

    g_sock = open_socket();
    if (g_sock < 0) {
        printf("stage5: socket unavailable\n");
        fflush(stdout);
        g_next_socket_try = now_ms() + 1000;
    } else {
        printf("stage5: socket ok fd=%d\n", g_sock);
        fflush(stdout);
    }

    Detour_Construct(&Detour_scePadRead, DetourMode_x32);
    Detour_DetourFunction(&Detour_scePadRead, (uint64_t)scePadRead, (void *)scePadRead_hook);

    Detour_Construct(&Detour_scePadReadState, DetourMode_x32);
    Detour_DetourFunction(&Detour_scePadReadState, (uint64_t)scePadReadState, (void *)scePadReadState_hook);

    printf("stage5: hooks installed\n");
    fflush(stdout);

    return 0;
}

int32_t attr_public plugin_unload(int32_t argc, const char *argv[])
{
    (void)argc;
    (void)argv;
    if (g_sock >= 0) {
        close(g_sock);
        g_sock = -1;
    }
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
