/*
 * Host tests for the wheel to pad mapping.
 *
 * Frames are the same real captures used by test_report.c, taken from
 * docs/captures/20261118-190530_ps3_separate.json.
 */

#include <stdio.h>
#include <string.h>

#include "../src/mapping.h"
#include "../src/report.h"

typedef struct {
    const char *name;
    uint8_t frame[VOLANTE_REPORT_LEN];
    uint32_t buttons;
    uint8_t left_x;
    uint8_t l2;
    uint8_t r2;
} fixture;

/* clang-format off */
static const fixture fixtures[] = {
    { "baseline",    { 0x00, 0xE0, 0xFF, 0x43, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0, 0x43, 0, 0 },
    { "steer_left",  { 0x00, 0xE0, 0xFF, 0x00, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0, 0x00, 0, 0 },
    { "steer_right", { 0x00, 0xE0, 0xFF, 0xFF, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0, 0xFF, 0, 0 },
    { "throttle",    { 0x00, 0xE0, 0xFF, 0x80, 0xFF, 0x00, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      VOLANTE_PAD_R2, 0x80, 0, 255 },
    { "brake",       { 0x00, 0xE0, 0xFF, 0x80, 0x00, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      VOLANTE_PAD_L2, 0x80, 255, 0 },
    { "button_1",    { 0x01, 0xE0, 0xFF, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      VOLANTE_PAD_SQUARE, 0x80, 0, 0 },
    { "button_2",    { 0x02, 0xE0, 0xFF, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      VOLANTE_PAD_CROSS, 0x80, 0, 0 },
    { "button_3",    { 0x04, 0xE0, 0xFF, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      VOLANTE_PAD_CIRCLE, 0x80, 0, 0 },
    { "button_4",    { 0x08, 0xE0, 0xFF, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      VOLANTE_PAD_TRIANGLE, 0x80, 0, 0 },
    { "paddle_up",   { 0x20, 0xE0, 0xFF, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      VOLANTE_PAD_R1, 0x80, 0, 0 },
    { "paddle_down", { 0x10, 0xE0, 0xFF, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      VOLANTE_PAD_L1, 0x80, 0, 0 },
    { "dpad_up",     { 0x00, 0xE0, 0x00, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      VOLANTE_PAD_UP, 0x80, 0, 0 },
    { "dpad_right",  { 0x00, 0xE0, 0x02, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      VOLANTE_PAD_RIGHT, 0x80, 0, 0 },
    { "dpad_down",   { 0x00, 0xE0, 0x04, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      VOLANTE_PAD_DOWN, 0x80, 0, 0 },
    { "dpad_left",   { 0x00, 0xE0, 0x06, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      VOLANTE_PAD_LEFT, 0x80, 0, 0 },
    { "se",          { 0x00, 0xE1, 0xFF, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      VOLANTE_PAD_OPTIONS, 0x80, 0, 0 },
    { "st",          { 0x00, 0xE2, 0xFF, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      VOLANTE_PAD_TOUCH_PAD, 0x80, 0, 0 },
    { "home",        { 0x00, 0xF0, 0xFF, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      VOLANTE_PAD_L3, 0x80, 0, 0 },
};
/* clang-format on */

static int failures;

static void check(const char *name, const char *field, unsigned got, unsigned want)
{
    if (got != want) {
        printf("  FAIL  %s %s: got 0x%02X want 0x%02X\n", name, field, got, want);
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

        if (volante_parse(item->frame, VOLANTE_REPORT_LEN, &wheel) != 0) {
            printf("  FAIL  %s: parser rejected a valid frame\n", item->name);
            failures++;
            continue;
        }
        volante_map(&wheel, &pad);

        check(item->name, "buttons", pad.buttons, item->buttons);
        check(item->name, "left_x", pad.left_x, item->left_x);
        check(item->name, "l2", pad.l2, item->l2);
        check(item->name, "r2", pad.r2, item->r2);
        check(item->name, "left_y", pad.left_y, VOLANTE_PAD_STICK_CENTRE);
    }

    /* Partial pedal travel must not set the digital trigger bit. */
    {
        uint8_t frame[VOLANTE_REPORT_LEN] = { 0 };
        volante_report wheel;
        volante_pad_state pad;

        frame[1] = 0xE0;
        frame[2] = 0xFF;
        frame[3] = 0x80;
        frame[4] = 0xFF;
        frame[5] = 0xE0; /* throttle barely touched */
        frame[6] = 0x80;

        if (volante_parse(frame, VOLANTE_REPORT_LEN, &wheel) == 0) {
            volante_map(&wheel, &pad);
            if (pad.buttons & VOLANTE_PAD_R2) {
                printf("  FAIL  light throttle set the R2 bit\n");
                failures++;
            }
            if (pad.r2 == 0) {
                printf("  FAIL  light throttle produced no analog travel\n");
                failures++;
            }
        }
    }

    if (failures == 0) {
        printf("ok: %d mapping fixtures, 0 failures\n", (int)count);
        return 0;
    }
    printf("%d failure(s) across %d mapping fixtures\n", failures, (int)count);
    return 1;
}
