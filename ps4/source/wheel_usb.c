/*
 * Reading the Thrustmaster wheel on the PS4.
 *
 * The wheel speaks a Thrustmaster vendor protocol. A control IN request
 * with bmRequestType 0xA1, bRequest 0x01 and wValue 0x0000 returns the
 * live state. The reply is 27 bytes in PS3 mode and 19 bytes in PC mode
 * (or in PS3 mode after the paddle-up + paddle-down + D-pad-up combo).
 *
 * See docs/REPORTS.md for the byte maps.
 */

#include "wheel_usb.h"

#include <stdio.h>
#include <string.h>

#include <orbis/Usbd.h>
#include <orbis/libkernel.h>

#include <Common.h>

#include <pthread.h>

#define STEP(...)            \
    do {                     \
        printf(__VA_ARGS__); \
        fflush(stdout);      \
    } while (0)

/* Vendor control request that returns the live wheel state. */
#define VOLANTE_REQ_TYPE 0xA1u
#define VOLANTE_REQ_STATE 0x01u
#define VOLANTE_REQ_VALUE 0x0000u

/* Vendor control request this wheel accepts as an acknowledgement. */
#define VOLANTE_INIT_TYPE 0x21u
#define VOLANTE_INIT_REQUEST 0x09u
#define VOLANTE_INIT_VALUE 0x0250u

/* A second reply type, selected by wValue. Logged while the pedal layout is
 * still unknown. */
#define VOLANTE_REQ_VALUE_AUX 0x0300u
#define VOLANTE_AUX_LEN 8

static libusb_device_handle *g_handle = NULL;
static pthread_t g_thread;
static volatile int g_running = 0;
static volatile int g_have_report = 0;
static volatile int g_available = 0;
static volatile int g_logged_report = 0;
static volatile int64_t g_last_report_ms = 0;
static uint8_t g_report[VOLANTE_CONSOLE_REPORT_LEN];
static int g_report_len = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static uint8_t g_interface_number = 0;

static int64_t now_ms(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (int64_t)tv.tv_usec / 1000;
}

static int is_wheel(uint16_t vendor, uint16_t product)
{
    /* The wheel reports a different product id depending on its mode switch,
     * and the pedal test needs the PC position, so any Thrustmaster device is
     * accepted while the layout is still being learned. */
    (void)product;
    return vendor == VOLANTE_VID;
}

/*
 * libSceUsbd.sprx is not loaded in a game process by default. Calling its
 * functions before it is loaded faults with PRX_NOT_RESOLVED_FUNCTION
 * (0xa0020101), so it is loaded explicitly first.
 */
static int load_usbd(void)
{
    char path[256];
    int handle = 0;
    int result;

    snprintf(path, sizeof(path), "/%s/common/lib/%s",
             sceKernelGetFsSandboxRandomWord(), "libSceUsbd.sprx");

    result = sys_dynlib_load_prx(path, &handle);
    STEP("[wheelpad] usb: load_prx = %d handle=%d\n", result, handle);
    return result;
}

/* Reads the interface number from the configuration descriptor. */
static void read_interface(libusb_device *device)
{
    struct libusb_config_descriptor *config = NULL;
    int32_t interface_index;

    if (sceUsbdGetActiveConfigDescriptor(device, &config) != 0 || config == NULL) {
        return;
    }

    for (interface_index = 0; interface_index < config->bNumInterfaces; interface_index++) {
        struct libusb_interface *interface = &config->interface[interface_index];

        if (interface->num_altsetting > 0) {
            g_interface_number = interface->altsetting[0].bInterfaceNumber;
            STEP("[wheelpad] usb: interface %d\n", (int)g_interface_number);
        }
    }

    sceUsbdFreeConfigDescriptor(config);
}

/*
 * Finds the wheel and opens it. The device address changes on every plug,
 * so it is matched by vendor and product id.
 */
static int open_wheel(void)
{
    libusb_device **list = NULL;
    int32_t count;
    int32_t index;

    count = sceUsbdGetDeviceList(&list);
    if (count < 0 || list == NULL) {
        STEP("[wheelpad] usb: device list failed (%d)\n", (int)count);
        return -1;
    }

    for (index = 0; index < count; index++) {
        struct libusb_device_descriptor descriptor;

        if (sceUsbdGetDeviceDescriptor(list[index], &descriptor) != 0) {
            continue;
        }
        if (!is_wheel(descriptor.idVendor, descriptor.idProduct)) {
            continue;
        }

        STEP("[wheelpad] usb: found vid=%04x pid=%04x\n",
             (int)descriptor.idVendor, (int)descriptor.idProduct);
        read_interface(list[index]);

        if (sceUsbdOpen(list[index], &g_handle) != 0 || g_handle == NULL) {
            STEP("[wheelpad] usb: open failed\n");
            g_handle = NULL;
            continue;
        }

        /*
         * The kernel HID driver owns the interface. Detach it so the
         * plugin can claim it.
         */
        sceUsbdDetachKernelDriver(g_handle, g_interface_number);

        if (sceUsbdClaimInterface(g_handle, g_interface_number) != 0) {
            STEP("[wheelpad] usb: claim interface %d failed\n", (int)g_interface_number);
            sceUsbdClose(g_handle);
            g_handle = NULL;
            continue;
        }

        STEP("[wheelpad] usb: claimed interface %d\n", (int)g_interface_number);
        sceUsbdFreeDeviceList(list);
        return 0;
    }

    sceUsbdFreeDeviceList(list);
    return -1;
}

static void close_wheel(void)
{
    if (g_handle == NULL) {
        return;
    }

    sceUsbdReleaseInterface(g_handle, g_interface_number);
    sceUsbdClose(g_handle);
    g_handle = NULL;
    g_available = 0;
    STEP("[wheelpad] usb: closed\n");
}

/*
 * Reads the live wheel state with the vendor control request. Returns the
 * number of bytes received, or a negative value on failure.
 */
static int32_t read_state(uint8_t *buffer, int32_t length)
{
    if (g_handle == NULL) {
        return -1;
    }

    return sceUsbdControlTransfer(g_handle, VOLANTE_REQ_TYPE, VOLANTE_REQ_STATE,
                                  VOLANTE_REQ_VALUE, (uint16_t)g_interface_number,
                                  buffer, (uint16_t)length, 100);
}

/* The wheel accepts this request and answers with an acknowledgement. */
static void send_init(void)
{
    uint8_t setup[8] = { 0x42, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    int32_t result;

    result = sceUsbdControlTransfer(g_handle, VOLANTE_INIT_TYPE, VOLANTE_INIT_REQUEST,
                                    VOLANTE_INIT_VALUE, (uint16_t)g_interface_number,
                                    setup, (uint16_t)sizeof(setup), 200);
    STEP("[wheelpad] usb: init = %d\n", (int)result);
}

/*
 * The wheel answers the state request with more than one reply shape. Only
 * the shape below carries live input: it ends with the constant status bytes
 * 02 00 02 00 02 00 02, and a live frame never has all of bytes 0 to 2 set.
 * Anything else is dropped, so a stray reply cannot press buttons in game.
 */
static const uint8_t k_state_tail[7] = { 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02 };

static int is_state_frame_len(const uint8_t *report, int32_t length)
{
    if (length == 27) {
        if (report[0] == 0xFF && report[1] == 0xFF && report[2] == 0xFF) {
            return 0;
        }
        return memcmp(report + 20, k_state_tail, sizeof(k_state_tail)) == 0;
    }

    if (length == 19) {
        /* Combo mode: pedals become analog axes. The real frame has the
         * button-high constant in the top three bits of byte 1 and 0x80 in
         * byte 6. Echo and phantom replies do not, so they are dropped. */
        if ((report[1] & 0xE0) != 0xE0) {
            return 0;
        }
        if (report[6] != 0x80) {
            return 0;
        }
        if (report[0] == 0xFF && report[1] == 0xFF && report[2] == 0xFF) {
            return 0;
        }
        return 1;
    }

    return 0;
}

/* Every distinct reply is logged while the layout is still being learned,
 * whichever shape it has, so a field that only shows up in the dropped shape
 * can still be found. Byte 18 moves on its own in every frame, so it is left
 * out of the comparison; it is still printed. */
static void log_raw(const uint8_t *report, int32_t length)
{
    static int logged = 0;
    static uint8_t last[64];
    static int32_t last_length = -1;
    int i;
    int quiet = 1;

    if (length <= 0 || length > 64) {
        return;
    }
    if (logged > 0 && last_length == length) {
        for (i = 0; i < length; ++i) {
            if (i == 18) {
                continue;
            }
            if (last[i] != report[i]) {
                quiet = 0;
                break;
            }
        }
        if (quiet) {
            return;
        }
    }
    if (logged >= 60) {
        return;
    }

    ++logged;
    last_length = length;
    memcpy(last, report, (size_t)length);

    STEP("[wheelpad] raw%s(%d):", is_state_frame_len(report, length) ? "" : " other", (int)length);
    for (i = 0; i < length; ++i) {
        STEP(" %02x", report[i]);
    }
    STEP("\n");
}

static void *poll_thread(void *argument)
{
    uint8_t buffer[64];
    int32_t length;

    (void)argument;

    while (g_running) {
        if (g_handle == NULL) {
            if (open_wheel() == 0) {
                g_available = 1;
                send_init();
            } else {
                sceKernelUsleep(500000);
                continue;
            }
        }

        length = read_state(buffer, (int32_t)sizeof(buffer));
        log_raw(buffer, (int32_t)length);
        if (is_state_frame_len(buffer, length)) {
            pthread_mutex_lock(&g_lock);
            memcpy(g_report, buffer, (size_t)length);
            if (length < (int32_t)sizeof(g_report)) {
                memset(g_report + length, 0, sizeof(g_report) - (size_t)length);
            }
            g_report_len = length;
            pthread_mutex_unlock(&g_lock);
            g_have_report = 1;
            g_last_report_ms = now_ms();
            g_available = 1;

            if (!g_logged_report) {
                g_logged_report = 1;
                STEP("[wheelpad] usb: first report, %d bytes\n", (int)length);
            }
        } else if (length > 0) {
            /* Another reply shape. Not input, keep the last good state. */
            static int logged_other = 0;

            if (!logged_other) {
                logged_other = 1;
                STEP("[wheelpad] usb: ignoring a non-state reply\n");
            }
        } else {
            /*
             * The read failed. If the wheel is gone, release the interface
             * straight away: the console's own HID stack tries to detach the
             * device when it is unplugged, and it fails if a plugin still
             * holds the claim. Holding on made SceSysCore crash.
             */
            if (sceUsbdCheckConnected(g_handle) != 0) {
                STEP("[wheelpad] usb: wheel disconnected, releasing the interface\n");
                close_wheel();
                g_have_report = 0;
                g_available = 0;
                g_logged_report = 0;
                continue;
            }

            /* A transient failure, the device is still there. */
            if (length < 0) {
                sceKernelUsleep(VOLANTE_POLL_US);
                continue;
            }
        }

        sceKernelUsleep(VOLANTE_POLL_US);
    }

    return NULL;
}

int volante_usb_start(void)
{
    if (g_running) {
        return 0;
    }

    if (load_usbd() != 0) {
        STEP("[wheelpad] usb: libSceUsbd did not load\n");
        return -1;
    }

    if (sceUsbdInit() < 0) {
        STEP("[wheelpad] usb: sceUsbdInit failed\n");
        return -1;
    }
    STEP("[wheelpad] usb: sceUsbdInit ok\n");

    g_running = 1;
    if (pthread_create(&g_thread, NULL, poll_thread, NULL) != 0) {
        g_running = 0;
        sceUsbdExit();
        STEP("[wheelpad] usb: thread create failed\n");
        return -1;
    }

    return 0;
}

void volante_usb_stop(void)
{
    if (!g_running) {
        return;
    }

    g_running = 0;
    pthread_join(g_thread, NULL);
    close_wheel();
    sceUsbdExit();
}

int volante_usb_available(void)
{
    return g_available;
}

int volante_usb_latest(volante_report *out)
{
    uint8_t copy[VOLANTE_CONSOLE_REPORT_LEN];
    int length;
    static int logged_no_report = 0;
    static int logged_stale = 0;
    static int logged_parse = 0;

    if (!g_have_report || out == NULL) {
        if (!logged_no_report) {
            logged_no_report = 1;
            STEP("[wheelpad] latest: no report yet\n");
        }
        return 0;
    }
    if (now_ms() - g_last_report_ms > 300) {
        if (!logged_stale) {
            logged_stale = 1;
            STEP("[wheelpad] latest: report is stale, %ld ms old\n",
                 (long)(now_ms() - g_last_report_ms));
        }
        return 0;
    }

    pthread_mutex_lock(&g_lock);
    memcpy(copy, g_report, VOLANTE_CONSOLE_REPORT_LEN);
    length = g_report_len;
    pthread_mutex_unlock(&g_lock);

    if (length < 19) {
        return 0;
    }

    if (volante_parse_console(copy, (size_t)length, out) != 0) {
        if (!logged_parse) {
            logged_parse = 1;
            STEP("[wheelpad] latest: parse failed\n");
        }
        return 0;
    }
    return 1;
}

int volante_usb_probe(void)
{
    libusb_device **list = NULL;
    int32_t count;
    int32_t index;
    int found = 0;

    /* The library must be loaded before any of its functions can be called. */
    if (load_usbd() != 0) {
        STEP("[wheelpad] usb: libSceUsbd did not load\n");
        return -1;
    }

    count = sceUsbdGetDeviceList(&list);
    if (count < 0 || list == NULL) {
        return count;
    }

    for (index = 0; index < count; index++) {
        struct libusb_device_descriptor descriptor;

        if (sceUsbdGetDeviceDescriptor(list[index], &descriptor) != 0) {
            continue;
        }
        STEP("[wheelpad] usb: device %04x:%04x\n",
             (int)descriptor.idVendor, (int)descriptor.idProduct);
        if (is_wheel(descriptor.idVendor, descriptor.idProduct)) {
            found = 1;
        }
    }

    sceUsbdFreeDeviceList(list);
    return found;
}
