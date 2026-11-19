/*
 * USB probe: opens the wheel and tries the possible ways of reading a
 * report, one at a time, logging the result of each. Nothing else runs in
 * the plugin, so a fault points at one specific call.
 *
 * The order is from the most likely to the least:
 *   1. interrupt transfer on the endpoint from the descriptor
 *   2. interrupt transfer on endpoint 0x81
 *   3. control transfer, HID GET_REPORT
 *   4. bulk transfer on the descriptor endpoint
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <orbis/Usbd.h>
#include <orbis/libkernel.h>

#include <Common.h>

#include "report.h"
#include "wheel_usb.h"

#define attr_module_hidden __attribute__((weak)) __attribute__((visibility("hidden")))
#define attr_public __attribute__((visibility("default")))

attr_public const char *g_pluginName = "wheelpad";
attr_public const char *g_pluginDesc = "usb read probe";
attr_public const char *g_pluginAuth = "volante";
attr_public uint32_t g_pluginVersion = 0x00000500; /* 5.00, usb probe */

#define STEP(...)            \
    do {                     \
        printf(__VA_ARGS__); \
        fflush(stdout);      \
    } while (0)

static libusb_device_handle *g_handle = NULL;
static uint8_t g_in_endpoint = VOLANTE_EP_FALLBACK;
static uint8_t g_out_endpoint = 0;
static uint8_t g_interface_number = 0;

static int64_t now_ms(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (int64_t)tv.tv_usec / 1000;
}

static void read_endpoints(libusb_device *device)
{
    struct libusb_config_descriptor *config = NULL;
    int32_t interface_index;
    int32_t alt;
    int32_t endpoint_index;

    if (sceUsbdGetActiveConfigDescriptor(device, &config) != 0 || config == NULL) {
        return;
    }

    for (interface_index = 0; interface_index < config->bNumInterfaces; interface_index++) {
        struct libusb_interface *interface = &config->interface[interface_index];

        for (alt = 0; alt < interface->num_altsetting; alt++) {
            struct libusb_interface_descriptor *descriptor = &interface->altsetting[alt];
            const unsigned char *raw;

            g_interface_number = descriptor->bInterfaceNumber;
            STEP("[probe] interface %d class=%d endpoints=%d\n",
                 (int)descriptor->bInterfaceNumber, (int)descriptor->bInterfaceClass,
                 (int)descriptor->bNumEndpoints);

            for (endpoint_index = 0; endpoint_index < descriptor->bNumEndpoints; endpoint_index++) {
                raw = (const unsigned char *)descriptor->endpoint + endpoint_index * 7;
                if (raw == NULL) {
                    break;
                }
                STEP("[probe]   endpoint 0x%02x attributes 0x%02x\n", (int)raw[0], (int)raw[3]);

                /*
                 * The wheel exposes a single interrupt endpoint whose
                 * address has the direction bit clear. Both directions are
                 * tried on it, so it is remembered as the shared endpoint.
                 */
                if (g_in_endpoint == VOLANTE_EP_FALLBACK) {
                    g_in_endpoint = raw[0];
                }
                if (g_out_endpoint == 0) {
                    g_out_endpoint = raw[0];
                }
            }
        }
    }

    sceUsbdFreeConfigDescriptor(config);
    STEP("[probe] chosen in endpoint 0x%02x on interface %d\n",
         (int)g_in_endpoint, (int)g_interface_number);
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

        if (sceUsbdGetDeviceDescriptor(list[index], &descriptor) != 0) {
            continue;
        }
        if (descriptor.idVendor != VOLANTE_VID) {
            continue;
        }
        if (descriptor.idProduct != VOLANTE_PID_PC && descriptor.idProduct != VOLANTE_PID_PS4) {
            continue;
        }

        STEP("[probe] found %04x:%04x\n", (int)descriptor.idVendor, (int)descriptor.idProduct);
        read_endpoints(list[index]);

        if (sceUsbdOpen(list[index], &g_handle) != 0 || g_handle == NULL) {
            STEP("[probe] open failed\n");
            g_handle = NULL;
            continue;
        }

        sceUsbdDetachKernelDriver(g_handle, g_interface_number);
        if (sceUsbdClaimInterface(g_handle, g_interface_number) != 0) {
            STEP("[probe] claim failed\n");
            sceUsbdClose(g_handle);
            g_handle = NULL;
            continue;
        }

        STEP("[probe] claimed interface %d\n", (int)g_interface_number);
        sceUsbdFreeDeviceList(list);
        return 0;
    }

    sceUsbdFreeDeviceList(list);
    return -1;
}

static void try_transfer(const char *name, uint8_t endpoint, int control, uint8_t request)
{
    uint8_t buffer[VOLANTE_REPORT_LEN];
    int32_t transferred = 0;
    int32_t result;
    int64_t started;

    memset(buffer, 0, sizeof(buffer));
    started = now_ms();

    if (control) {
        result = sceUsbdControlTransfer(g_handle, 0x81, request, 0x0100,
                                        (uint16_t)g_interface_number, buffer,
                                        (uint16_t)sizeof(buffer), 100);
        transferred = result == 0 ? (int32_t)sizeof(buffer) : 0;
    } else {
        result = sceUsbdInterruptTransfer(g_handle, endpoint, buffer,
                                          (int32_t)sizeof(buffer), &transferred, 100);
    }

    STEP("[probe] %s: result=%d transferred=%d elapsed=%lldms first=%02x %02x %02x %02x\n",
         name, (int)result, (int)transferred, (long long)(now_ms() - started),
         (int)buffer[0], (int)buffer[1], (int)buffer[2], (int)buffer[3]);
}

/*
 * Reads a HID report over the control pipe. The report type goes in the high
 * byte of wValue and the report id in the low byte.
 */
static void try_control_report(const char *name, uint8_t report_type, uint8_t report_id)
{
    uint8_t buffer[VOLANTE_REPORT_LEN];
    int32_t result;

    memset(buffer, 0, sizeof(buffer));

    result = sceUsbdControlTransfer(g_handle, 0x81, 0x01,
                                    (uint16_t)((report_type << 8) | report_id),
                                    (uint16_t)g_interface_number, buffer,
                                    (uint16_t)sizeof(buffer), 200);

    STEP("[probe] %s: result=%d first=%02x %02x %02x %02x %02x %02x %02x %02x\n",
         name, (int)result,
         (int)buffer[0], (int)buffer[1], (int)buffer[2], (int)buffer[3],
         (int)buffer[4], (int)buffer[5], (int)buffer[6], (int)buffer[7]);
}

int32_t attr_public plugin_load(int32_t argc, const char *argv[])
{
    char path[256];
    int handle = 0;
    uint8_t descriptor[256];
    int32_t length;
    int32_t index;

    (void)argc;
    (void)argv;

    snprintf(path, sizeof(path), "/%s/common/lib/%s",
             sceKernelGetFsSandboxRandomWord(), "libSceUsbd.sprx");
    STEP("[probe] load_prx = %d\n", sys_dynlib_load_prx(path, &handle));
    STEP("[probe] sceUsbdInit = %d\n", sceUsbdInit());

    if (open_wheel() != 0) {
        STEP("[probe] no wheel\n");
        return 0;
    }

    /*
     * The HID report descriptor says which report types exist, how long they
     * are and which report id carries the input. Without it the read method
     * is guesswork.
     *
     * sceUsbdGetDescriptor does not work on this device, but the standard
     * HID request GET_DESCRIPTOR over the control pipe does, so it is sent
     * by hand.
     */
    {
        uint8_t request[8];

        memset(request, 0, sizeof(request));
        request[0] = 0x06; /* bmRequestType: IN, standard, device */
        request[1] = 0x21; /* bRequest: HID GET_DESCRIPTOR */
        request[2] = 0x00; /* wValue high: report type */
        request[3] = 0x22; /* wValue low: report descriptor */
        request[4] = 0x00;
        request[5] = 0x00; /* wIndex: interface */
        request[6] = 0x00;
        request[7] = 0x00;

        length = sceUsbdControlTransfer(g_handle, 0x80, 0x06, 0x0220,
                                        (uint16_t)g_interface_number, descriptor,
                                        (uint16_t)sizeof(descriptor), 200);
        STEP("[probe] report descriptor length=%d\n", (int)length);
        for (index = 0; index < length && index < (int32_t)sizeof(descriptor); index++) {
            STEP("[probe] rd[%d] = 0x%02x\n", (int)index, (int)descriptor[index]);
        }
    }

    /*
     * Thrustmaster wheels need an init sequence before they answer. The
     * Linux kernel driver (drivers/hid/hid-thrustmaster.c) does it in
     * three steps:
     *
     *   1. a sequence of setup packets on the interrupt out endpoint
     *   2. a control IN request, bRequest 73, to read the wheel model
     *   3. a control OUT request, bRequest 83, to switch the wheel to its
     *      full capabilities
     *
     * The setup packets matter most: without them the device stalls every
     * transfer.
     */
    {
        static const uint8_t setup_0[] = { 0x42, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        static const uint8_t setup_1[] = { 0x0a, 0x04, 0x90, 0x03, 0x00, 0x00, 0x00, 0x00 };
        static const uint8_t setup_2[] = { 0x0a, 0x04, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00 };
        static const uint8_t setup_3[] = { 0x0a, 0x04, 0x12, 0x10, 0x00, 0x00, 0x00, 0x00 };
        static const uint8_t setup_4[] = { 0x0a, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00 };
        static const uint8_t *const setup[] = { setup_0, setup_1, setup_2, setup_3, setup_4 };
        static const uint32_t setup_size[] = {
            sizeof(setup_0), sizeof(setup_1), sizeof(setup_2), sizeof(setup_3), sizeof(setup_4)
        };
        uint8_t reply[32];
        uint32_t index;
        int32_t transferred = 0;
        int32_t result;

        for (index = 0; index < 5; index++) {
            /*
             * The setup packets go to an interrupt out endpoint. This
             * device exposes a single endpoint whose address has the
             * direction bit clear, so both 0x07 and 0x87 are tried.
             */
            static const uint8_t addresses[] = { 0x07, 0x87 };

            uint32_t a;

            for (a = 0; a < 2; a++) {
                result = sceUsbdInterruptTransfer(g_handle, addresses[a],
                                                  (unsigned char *)setup[index],
                                                  (int32_t)setup_size[index], &transferred, 200);
                STEP("[probe] setup %u on 0x%02x: result=%d transferred=%d\n",
                     (unsigned)index, (int)addresses[a], (int)result, (int)transferred);
            }
        }

        /*
         * These are the control requests that the wheel answered earlier:
         * a vendor request with the byte 0x42, and a vendor read. The
         * vendor read returns the live wheel state, so it is repeated a
         * few times to see which bytes change.
         */
        {
            uint8_t setup[8] = { 0x42, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
            uint8_t reply[64];
            int32_t index;
            int32_t round;

            memset(reply, 0, sizeof(reply));
            result = sceUsbdControlTransfer(g_handle, 0x21, 0x09, 0x0250,
                                            (uint16_t)g_interface_number, setup,
                                            (uint16_t)sizeof(setup), 200);
            STEP("[probe] control vendor 42: result=%d\n", (int)result);

            for (round = 0; round < 4; round++) {
                memset(reply, 0, sizeof(reply));
                result = sceUsbdControlTransfer(g_handle, 0xA1, 0x01, 0x0000,
                                                (uint16_t)g_interface_number, reply,
                                                (uint16_t)sizeof(reply), 200);
                STEP("[probe] state read %d: result=%d\n", (int)round, (int)result);
                for (index = 0; index < 32; index++) {
                    STEP("[probe] s%d[%d] = 0x%02x\n", (int)round, (int)index, (int)reply[index]);
                }
                sceKernelUsleep(50000);
            }
        }

        memset(reply, 0, sizeof(reply));
        result = sceUsbdControlTransfer(g_handle, 0xC1, 73, 0x0000, 0x0000,
                                        reply, 0x0010, 200);
        STEP("[probe] model request 73: result=%d first=%02x %02x %02x %02x %02x %02x\n",
             (int)result, (int)reply[0], (int)reply[1], (int)reply[2],
             (int)reply[3], (int)reply[4], (int)reply[5]);

        /* The switch value is per wheel model. 0x0002 and 0x0005 cover the
         * models the kernel driver knows. */
        {
            static const uint16_t switches[] = { 0x0002, 0x0005 };
            uint32_t s;

            for (s = 0; s < 2; s++) {
                result = sceUsbdControlTransfer(g_handle, 0x41, 83, switches[s], 0x0000,
                                                NULL, 0, 200);
                STEP("[probe] switch request 83 value=%04x: result=%d\n",
                     (unsigned)switches[s], (int)result);
            }
        }
    }

    /*
     * The wheel answers a vendor IN request with bRequest 0x01 and the
     * reply carries the live state: byte 3 is the steering axis, bytes 4
     * to 6 are the other axes. The reply is read in a tight loop and only
     * the bytes that change are printed, so the pedals and buttons are
     * easy to pick out.
     */
    {
        uint8_t reply[64];
        uint8_t previous[64];
        int32_t index;
        int32_t round;
        int32_t changed;
        int32_t result;

        memset(previous, 0, sizeof(previous));

        for (round = 0; round < 600; round++) {
            memset(reply, 0, sizeof(reply));
            result = sceUsbdControlTransfer(g_handle, 0xA1, 0x01, 0x0000,
                                            (uint16_t)g_interface_number, reply,
                                            (uint16_t)sizeof(reply), 100);
            if (result != 27) {
                STEP("[probe] state read %d failed: result=%d\n", (int)round, (int)result);
                continue;
            }

            changed = 0;
            for (index = 0; index < 27; index++) {
                if (reply[index] != previous[index]) {
                    STEP("[probe] r%d byte %d: 0x%02x -> 0x%02x\n",
                         (int)round, (int)index, (int)previous[index], (int)reply[index]);
                    previous[index] = reply[index];
                    changed++;
                }
            }
            if (changed == 0 && round > 0 && (round % 100) == 0) {
                STEP("[probe] r%d steady\n", (int)round);
            }
            sceKernelUsleep(10000);
        }

        STEP("[probe] state loop done\n");
    }

    STEP("[probe] try 1: interrupt on the descriptor endpoint\n");
    try_transfer("interrupt descriptor", g_in_endpoint, 0, 0);

    STEP("[probe] try 2: interrupt on 0x81\n");
    try_transfer("interrupt 0x81", 0x81, 0, 0);

    STEP("[probe] try 2b: interrupt on 0x87\n");
    try_transfer("interrupt 0x87", 0x87, 0, 0);

    STEP("[probe] try 3: control GET_REPORT\n");
    try_transfer("control get_report", 0, 1, 0x01);

    STEP("[probe] try 3b: control GET_REPORT with report id 1\n");
    try_control_report("control get_report id1", 0x01, 0x01);

    STEP("[probe] try 3c: control GET_REPORT feature\n");
    try_control_report("control get_report feature", 0x03, 0x00);

    /*
     * The wheel answered a vendor control request earlier. The same request
     * is repeated here with the full reply dumped, because the reply may be
     * the input report itself.
     */
    {
        uint8_t setup[8] = { 0x42, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        uint8_t reply[64];
        int32_t result;
        int32_t index;

        memset(reply, 0, sizeof(reply));
        result = sceUsbdControlTransfer(g_handle, 0xA1, 0x09, 0x0250,
                                        (uint16_t)g_interface_number, reply,
                                        (uint16_t)sizeof(reply), 200);
        STEP("[probe] control read reply: result=%d\n", (int)result);
        for (index = 0; index < 32; index++) {
            STEP("[probe] reply[%d] = 0x%02x\n", (int)index, (int)reply[index]);
        }
    }

    STEP("[probe] try 4: bulk on the descriptor endpoint\n");
    {
        uint8_t buffer[VOLANTE_REPORT_LEN];
        int32_t transferred = 0;
        int32_t result;

        memset(buffer, 0, sizeof(buffer));
        result = sceUsbdBulkTransfer(g_handle, g_in_endpoint, buffer,
                                     (int32_t)sizeof(buffer), &transferred, 100);
        STEP("[probe] bulk descriptor: result=%d transferred=%d\n", (int)result, (int)transferred);
    }

    STEP("[probe] done\n");
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
