/*
 * Controlled capture probe.
 *
 * Reads the wheel state in a loop and prints only the bytes that changed,
 * so one control can be tested at a time. A marker line is printed every
 * second so the log can be split into one section per test action.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <orbis/Usbd.h>
#include <orbis/libkernel.h>

#include <Common.h>

#include "report.h"

#define attr_module_hidden __attribute__((weak)) __attribute__((visibility("hidden")))
#define attr_public __attribute__((visibility("default")))

attr_public const char *g_pluginName = "wheelpad";
attr_public const char *g_pluginDesc = "controlled capture";
attr_public const char *g_pluginAuth = "volante";
attr_public uint32_t g_pluginVersion = 0x00000600; /* 6.00, controlled capture */

#define STEP(...)            \
    do {                     \
        printf(__VA_ARGS__); \
        fflush(stdout);      \
    } while (0)

#define VOLANTE_VID 0x044F
#define VOLANTE_PID 0xB658
#define REPORT_LEN 27

static libusb_device_handle *g_handle = NULL;
static uint8_t g_interface_number = 0;

static int64_t now_ms(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (int64_t)tv.tv_usec / 1000;
}

static int open_wheel(void)
{
    libusb_device **list = NULL;
    int32_t count;
    int32_t index;

    count = sceUsbdGetDeviceList(&list);
    if (count < 0 || list == NULL) {
        return -1;
    }

    for (index = 0; index < count; index++) {
        struct libusb_device_descriptor descriptor;
        struct libusb_config_descriptor *config = NULL;
        int32_t interface_index;

        if (sceUsbdGetDeviceDescriptor(list[index], &descriptor) != 0) {
            continue;
        }
        if (descriptor.idVendor != VOLANTE_VID || descriptor.idProduct != VOLANTE_PID) {
            continue;
        }

        if (sceUsbdGetActiveConfigDescriptor(list[index], &config) == 0 && config != NULL) {
            for (interface_index = 0; interface_index < config->bNumInterfaces; interface_index++) {
                struct libusb_interface *interface = &config->interface[interface_index];

                if (interface->num_altsetting > 0) {
                    g_interface_number = interface->altsetting[0].bInterfaceNumber;
                }
            }
            sceUsbdFreeConfigDescriptor(config);
        }

        if (sceUsbdOpen(list[index], &g_handle) != 0 || g_handle == NULL) {
            g_handle = NULL;
            continue;
        }

        sceUsbdDetachKernelDriver(g_handle, g_interface_number);
        if (sceUsbdClaimInterface(g_handle, g_interface_number) != 0) {
            sceUsbdClose(g_handle);
            g_handle = NULL;
            continue;
        }

        sceUsbdFreeDeviceList(list);
        return 0;
    }

    sceUsbdFreeDeviceList(list);
    return -1;
}

int32_t attr_public plugin_load(int32_t argc, const char *argv[])
{
    uint8_t reply[64];
    uint8_t previous[64];
    char path[256];
    int handle = 0;
    int64_t started;
    int64_t last_marker = 0;
    int32_t round;

    (void)argc;
    (void)argv;

    snprintf(path, sizeof(path), "/%s/common/lib/%s",
             sceKernelGetFsSandboxRandomWord(), "libSceUsbd.sprx");
    STEP("[cap] load_prx = %d\n", sys_dynlib_load_prx(path, &handle));
    STEP("[cap] sceUsbdInit = %d\n", sceUsbdInit());

    if (open_wheel() != 0) {
        STEP("[cap] no wheel\n");
        return 0;
    }
    STEP("[cap] wheel open, interface %d\n", (int)g_interface_number);

    memset(previous, 0, sizeof(previous));
    started = now_ms();

    for (round = 0; round < 3000; round++) {
        int32_t index;
        int32_t length;

        length = sceUsbdControlTransfer(g_handle, 0xA1, 0x01, 0x0000,
                                        (uint16_t)g_interface_number, reply,
                                        (uint16_t)sizeof(reply), 100);
        if (length != REPORT_LEN) {
            STEP("[cap] read failed: %d\n", (int)length);
            sceKernelUsleep(100000);
            continue;
        }

        for (index = 0; index < REPORT_LEN; index++) {
            if (reply[index] != previous[index]) {
                STEP("[cap] t=%6lld b%02d: %02x -> %02x\n",
                     (long long)((now_ms() - started) / 100), (int)index,
                     (int)previous[index], (int)reply[index]);
                previous[index] = reply[index];
            }
        }

        if (now_ms() - last_marker >= 1000) {
            last_marker = now_ms();
            STEP("[cap] t=%6lld marker\n", (long long)((now_ms() - started) / 100));
        }

        sceKernelUsleep(10000);
    }

    STEP("[cap] done\n");
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
