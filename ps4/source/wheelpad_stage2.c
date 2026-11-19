/*
 * Stage 2 probe: stage 1 plus opening a UDP socket at load, the same way the
 * real plugin does. No pad hooks yet.
 */

#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define attr_module_hidden __attribute__((weak)) __attribute__((visibility("hidden")))
#define attr_public __attribute__((visibility("default")))

attr_public const char *g_pluginName = "wheelpad";
attr_public const char *g_pluginDesc = "stage 2 probe";
attr_public const char *g_pluginAuth = "volante";
attr_public uint32_t g_pluginVersion = 0x00000103; /* 1.03, stage 2 */

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
    (void)argc;
    (void)argv;

    logline("stage2: enter");
    printf("stage2: enter\n");
    fflush(stdout);

    g_sock = open_socket();
    if (g_sock < 0) {
        logline("stage2: socket unavailable");
        printf("stage2: socket unavailable\n");
        fflush(stdout);
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "stage2: socket ok fd=%d", g_sock);
        logline(buf);
        printf("stage2: socket ok fd=%d\n", g_sock);
        fflush(stdout);
    }

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
