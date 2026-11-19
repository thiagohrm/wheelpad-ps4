/*
 * Stage 3 probe: stage 2 plus the HOOK32 detours from the real plugin, but
 * without merge_wheel or pump_socket. This isolates whether Detour itself
 * crashes at load.
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
attr_public const char *g_pluginDesc = "stage 3 probe";
attr_public const char *g_pluginAuth = "volante";
attr_public uint32_t g_pluginVersion = 0x00000104; /* 1.04, stage 3 */

HOOK_INIT(scePadRead);
HOOK_INIT(scePadReadState);

int32_t scePadRead_hook(int32_t handle, OrbisPadData *data, int32_t count)
{
    (void)handle;
    (void)data;
    (void)count;
    return 0;
}

int32_t scePadReadState_hook(int32_t handle, OrbisPadData *data)
{
    (void)handle;
    (void)data;
    return 0;
}

static int g_sock = -1;

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

int32_t attr_public plugin_load(int32_t argc, const char *argv[])
{
    void *stub;

    (void)argc;
    (void)argv;

    logline("stage3: enter");
    printf("stage3: enter\n");
    fflush(stdout);

    g_sock = open_socket();
    if (g_sock < 0) {
        logline("stage3: socket unavailable");
        printf("stage3: socket unavailable\n");
        fflush(stdout);
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "stage3: socket ok fd=%d", g_sock);
        logline(buf);
        printf("stage3: socket ok fd=%d\n", g_sock);
        fflush(stdout);
    }

    logline("stage3: hook read");
    printf("stage3: hook read\n");
    fflush(stdout);
    Detour_Construct(&Detour_scePadRead, DetourMode_x32);
    stub = Detour_DetourFunction(&Detour_scePadRead, (uint64_t)scePadRead, (void *)scePadRead_hook);
    logline("stage3: hook read done");
    printf("stage3: hook read done %p\n", stub);
    fflush(stdout);

    logline("stage3: hook state");
    printf("stage3: hook state\n");
    fflush(stdout);
    Detour_Construct(&Detour_scePadReadState, DetourMode_x32);
    stub = Detour_DetourFunction(&Detour_scePadReadState, (uint64_t)scePadReadState, (void *)scePadReadState_hook);
    logline("stage3: hook state done");
    printf("stage3: hook state done %p\n", stub);
    fflush(stdout);

    logline("stage3: done");
    printf("stage3: done\n");
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
