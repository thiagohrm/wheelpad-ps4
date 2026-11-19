/*
 * Host tests for the PS4 flavour of the wheel report.
 *
 * The frames come from real console captures. The console layout is
 * documented in docs/REPORTS.md.
 */

#include <stdio.h>
#include <string.h>

#include "../src/mapping.h"
#include "../src/report.h"

typedef struct {
    const char *name;
    uint8_t frame[VOLANTE_CONSOLE_REPORT_LEN];
    uint32_t buttons;
    uint8_t steering;
    uint8_t brake;
    uint8_t throttle;
    uint8_t hat;
} fixture;

/* clang-format off */
static const fixture fixtures[] = {
    /* Idle, wheel centred, pedals released. */
    { "idle",
      { 0xC0, 0xE0, 0xFF, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02,
        0x00, 0x02, 0x00 },
      0, 0x80, 255, 255, 0x0F },

    /* Steering to the left. */
    { "steer_left",
      { 0xC0, 0xE0, 0xFF, 0x40, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02,
        0x00, 0x02, 0x00 },
      0, 0x40, 255, 255, 0x0F },

    /* Steering to the right. */
    { "steer_right",
      { 0xC0, 0xE0, 0xFF, 0xC0, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02,
        0x00, 0x02, 0x00 },
      0, 0xC0, 255, 255, 0x0F },

    /* Brake fully pressed, throttle released. */
    { "brake",
      { 0xC0, 0xE0, 0xFF, 0x80, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02,
        0x00, 0x02 },
      0, 0x80, 0, 255, 0x0F },

    /* Throttle partly pressed, brake released. */
    { "throttle",
      { 0xC0, 0xE0, 0xFF, 0x80, 0x80, 0x70, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02,
        0x00, 0x02 },
      0, 0x80, 255, 224, 0x0F },

    /* A wheel button held: bytes 0 to 2 drop to zero and the vendor
     * mirrors fill in. */
    { "button_held",
      { 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x70, 0x0D, 0x02, 0x00, 0x22, 0x27,
        0x23, 0x04, 0x04, 0x3F, 0x00, 0x01, 0x01, 0x00, 0x00, 0x14, 0x14, 0x51,
        0x01, 0x00, 0x08 },
      0, 0x40, 0, 0, 0x00 },

    /* D-pad left: hat byte reads 0x06. */
    { "dpad_left",
      { 0xC0, 0xE0, 0x06, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02,
        0x00, 0x02, 0x00 },
      0, 0x80, 255, 255, 6 },
};
/* clang-format on */

static int failures;

static void check(const char *name, const char *field, unsigned got, unsigned want)
{
    if (got != want) {
        printf("  FAIL  %s %s: got %u want %u\n", name, field, got, want);
        failures++;
    }
}

int main(void)
{
    size_t index;
    size_t count = sizeof(fixtures) / sizeof(fixtures[0]);

    for (index = 0; index < count; index++) {
        const fixture *item = &fixtures[index];
        volante_report wheel;
        volante_pad_state pad;

        memset(&wheel, 0, sizeof(wheel));
        memset(&pad, 0, sizeof(pad));

        if (volante_parse_console(item->frame, VOLANTE_CONSOLE_REPORT_LEN, &wheel) != 0) {
            printf("  FAIL  %s: parser rejected a valid frame\n", item->name);
            failures++;
            continue;
        }

        check(item->name, "buttons", wheel.buttons, item->buttons);
        check(item->name, "steering", wheel.steering, item->steering);
        check(item->name, "brake", wheel.brake, item->brake);
        check(item->name, "throttle", wheel.throttle, item->throttle);
        check(item->name, "hat", wheel.hat, item->hat);

        /* The mapping layer must work on the console values too. */
        volante_map(&wheel, &pad);
        check(item->name, "pad.left_x", pad.left_x, item->steering);
    }

    /* A short frame must be refused. */
    {
        {
            uint8_t short_frame[10];
            volante_report wheel;

            memset(short_frame, 0, sizeof(short_frame));
            memset(&wheel, 0, sizeof(wheel));
            if (volante_parse_console(short_frame, sizeof(short_frame), &wheel) == 0) {
                printf("  FAIL  a short frame was accepted\n");
                failures++;
            }
        }
    }

    if (failures == 0) {
        printf("ok: %d console fixtures, 0 failures\n", (int)count);
        return 0;
    }
    printf("%d failure(s) across %d console fixtures\n", failures, (int)count);
    return 1;
}
