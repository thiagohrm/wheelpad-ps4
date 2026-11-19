/*
 * wheelpad - GoldHEN plugin that feeds wheel input into the PS4 pad stream.
 *
 * The wheel is plugged into the console, so the plugin reads it directly with
 * libSceUsbd, the console's libusb wrapper. A poll thread keeps the newest
 * HID report, the tested parser turns it into wheel state, and the mapping
 * layer merges that into what the game reads from scePadReadState.
 *
 * Parsing and mapping live in src/report.c and src/mapping.c, shared with the
 * host tests, so there is only one implementation of the report layout.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <orbis/Pad.h>
#include <orbis/_types/pad.h>
#include <orbis/libkernel.h>

#include <Common.h>

#include "mapping.h"
#include "report.h"
#include "wheel_usb.h"

#define attr_module_hidden __attribute__((weak)) __attribute__((visibility("hidden")))
#define attr_public __attribute__((visibility("default")))

attr_public const char *g_pluginName = "wheelpad";
attr_public const char *g_pluginDesc = "Thrustmaster wheel to pad";
attr_public const char *g_pluginAuth = "volante";
attr_public uint32_t g_pluginVersion = 0x00000300; /* 3.00, analog pedals over the interrupt endpoint */

/* The mapping layer repeats these numbers so host tests need no Orbis
 * headers. If one ever drifts, the build stops here instead of the game. */
typedef char volante_check_l2[(VOLANTE_PAD_L2 == ORBIS_PAD_BUTTON_L2) ? 1 : -1];
typedef char volante_check_r2[(VOLANTE_PAD_R2 == ORBIS_PAD_BUTTON_R2) ? 1 : -1];
typedef char volante_check_l1[(VOLANTE_PAD_L1 == ORBIS_PAD_BUTTON_L1) ? 1 : -1];
typedef char volante_check_r1[(VOLANTE_PAD_R1 == ORBIS_PAD_BUTTON_R1) ? 1 : -1];
typedef char volante_check_cross[(VOLANTE_PAD_CROSS == ORBIS_PAD_BUTTON_CROSS) ? 1 : -1];
typedef char volante_check_circle[(VOLANTE_PAD_CIRCLE == ORBIS_PAD_BUTTON_CIRCLE) ? 1 : -1];
typedef char volante_check_square[(VOLANTE_PAD_SQUARE == ORBIS_PAD_BUTTON_SQUARE) ? 1 : -1];
typedef char volante_check_triangle[(VOLANTE_PAD_TRIANGLE == ORBIS_PAD_BUTTON_TRIANGLE) ? 1 : -1];
typedef char volante_check_options[(VOLANTE_PAD_OPTIONS == ORBIS_PAD_BUTTON_OPTIONS) ? 1 : -1];
typedef char volante_check_l3[(VOLANTE_PAD_L3 == ORBIS_PAD_BUTTON_L3) ? 1 : -1];
typedef char volante_check_up[(VOLANTE_PAD_UP == ORBIS_PAD_BUTTON_UP) ? 1 : -1];
typedef char volante_check_down[(VOLANTE_PAD_DOWN == ORBIS_PAD_BUTTON_DOWN) ? 1 : -1];
typedef char volante_check_left[(VOLANTE_PAD_LEFT == ORBIS_PAD_BUTTON_LEFT) ? 1 : -1];
typedef char volante_check_right[(VOLANTE_PAD_RIGHT == ORBIS_PAD_BUTTON_RIGHT) ? 1 : -1];
typedef char volante_check_touch[(VOLANTE_PAD_TOUCH_PAD == ORBIS_PAD_BUTTON_TOUCH_PAD) ? 1 : -1];

HOOK_INIT(scePadReadState);

#define STEP(...)            \
    do {                     \
        printf(__VA_ARGS__); \
        fflush(stdout);      \
    } while (0)

static int g_logged_live = 0;
static int g_logged_hook = 0;
static int g_logged_entry = 0;

static void merge_wheel(OrbisPadData *data)
{
    volante_report wheel;
    volante_pad_state pad;

    if (data == NULL) {
        return;
    }
    if (!volante_usb_latest(&wheel)) {
        if (g_logged_live) {
            STEP("[wheelpad] no reports, releasing the wheel\n");
            g_logged_live = 0;
        }
        return;
    }

    volante_map(&wheel, &pad);

    if (!g_logged_live) {
        STEP("[wheelpad] wheel live, steering=%u\n", (unsigned)pad.left_x);
        g_logged_live = 1;
    }

    /* A short state trace: what the game actually receives, so a wrong byte
     * role shows up as a value that never matches the real wheel. */
    {
        static uint32_t last_buttons = 0xFFFFFFFFu;
        static uint8_t last_x = 0xFF;
        static uint8_t last_l2 = 0xFF;
        static uint8_t last_r2 = 0xFF;
        static uint8_t last_hat = 0xFF;
        static int traced = 0;

        if (traced < 40 && (pad.buttons != last_buttons || pad.left_x != last_x ||
                            pad.l2 != last_l2 || pad.r2 != last_r2 ||
                            wheel.hat != last_hat)) {
            STEP("[wheelpad] state: x=%u l2=%u r2=%u hat=%u buttons=0x%x\n",
                 (unsigned)pad.left_x, (unsigned)pad.l2, (unsigned)pad.r2,
                 (unsigned)wheel.hat, (unsigned)pad.buttons);
            last_buttons = pad.buttons;
            last_x = pad.left_x;
            last_l2 = pad.l2;
            last_r2 = pad.r2;
            last_hat = wheel.hat;
            ++traced;
        }
    }

    /* Steering and triggers come from the wheel. Vertical stick and every
     * button still pressed on the controller keep working, so menus stay
     * usable with the pad in hand. */
    data->leftStick.x = pad.left_x;
    data->analogButtons.l2 = pad.l2;
    data->analogButtons.r2 = pad.r2;
    data->buttons |= pad.buttons;
}

int32_t scePadReadState_hook(int32_t handle, OrbisPadData *data)
{
    int32_t result;

    /* Logged before calling the original, so a stall inside the original
     * trampoline can be told apart from the hook never running. */
    if (!g_logged_entry) {
        g_logged_entry = 1;
        STEP("[wheelpad] hook entered\n");
    }

    result = HOOK_CONTINUE(scePadReadState, int32_t (*)(int32_t, OrbisPadData *), handle, data);

    if (!g_logged_hook) {
        STEP("[wheelpad] hook: readState called handle=%ld result=%ld\n",
             (long)handle, (long)result);
        g_logged_hook = 1;
    }

    if (result == 0) {
        merge_wheel(data);
    }
    return result;
}

/*
 * The imported scePadReadState symbol in this module points at our own PLT
 * entry, not at libScePad. Hooking that would only catch our own calls, so
 * resolve the real function in libScePad first.
 */
static uint64_t resolve_pad_symbol(const char *name)
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

/*
 * On this firmware the exported pad functions are thunks:
 *
 *     31 d2            xor edx, edx
 *     e9 49 ec ff ff   jmp real_function
 *     90 ...           padding
 *
 * The detour copies the first bytes of the function into its trampoline and
 * jumps back after them. A relative jump copied to a different address lands
 * somewhere else, and the calling thread then hangs, so the thunk must be
 * followed and the real function hooked instead.
 */
static uint64_t follow_thunks(uint64_t address)
{
    int hop;

    for (hop = 0; hop < 8 && address != 0; ++hop) {
        const uint8_t *base = (const uint8_t *)address;
        const uint8_t *jump;
        int32_t offset;

        if (base[0] == 0xE9) {
            jump = base;
        } else if (base[0] == 0x31 && (base[1] == 0xD2 || base[1] == 0xC9) && base[2] == 0xE9) {
            jump = base + 2;
        } else {
            break;
        }

        memcpy(&offset, jump + 1, sizeof(offset));
        address = (uint64_t)((int64_t)((uint64_t)jump + 5) + offset);
    }

    return address;
}

int32_t attr_public plugin_load(int32_t argc, const char *argv[])
{
    uint64_t state_address;
    void *stub;

    (void)argc;
    (void)argv;

    STEP("[wheelpad] loading\n");

    STEP("[wheelpad] probing the usb bus\n");
    volante_usb_probe();

    STEP("[wheelpad] starting the wheel reader\n");
    if (volante_usb_start() != 0) {
        STEP("[wheelpad] usb start failed\n");
    }

    STEP("[wheelpad] resolving scePadReadState\n");
    state_address = resolve_pad_symbol("scePadReadState");
    STEP("[wheelpad] resolved readState=0x%lx\n", (long)state_address);
    state_address = follow_thunks(state_address);
    STEP("[wheelpad] hooking the function itself at 0x%lx\n", (long)state_address);
    if (state_address != 0) {
        const uint8_t *code = (const uint8_t *)state_address;
        int i;

        STEP("[wheelpad] prologue:");
        for (i = 0; i < 32; ++i) {
            STEP(" %02x", code[i]);
        }
        STEP("\n");
    }

    /*
     * x64 mode is required. The x32 mode of this SDK builds its trampoline
     * with malloc, and plugin heap memory is not executable, so the game
     * faults the moment it calls the hooked function. x64 mode gets its
     * trampoline from sceKernelMmap with VM_PROT_ALL instead.
     */
    Detour_Construct(&Detour_scePadReadState, DetourMode_x64);
    stub = Detour_DetourFunction(&Detour_scePadReadState,
                                 state_address != 0 ? state_address : (uint64_t)scePadReadState,
                                 (void *)scePadReadState_hook);
    if (stub == NULL) {
        STEP("[wheelpad] hook readState failed, the wheel stays unread\n");
    } else {
        STEP("[wheelpad] hook readState stub=0x%lx\n", (long)stub);
    }

    STEP("[wheelpad] ready\n");
    return 0;
}

int32_t attr_public plugin_unload(int32_t argc, const char *argv[])
{
    (void)argc;
    (void)argv;

    STEP("[wheelpad] unloading\n");
    UNHOOK(scePadReadState);
    volante_usb_stop();
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
