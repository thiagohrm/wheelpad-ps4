/*
 * Host tests for the wheel report parser.
 *
 * Fixtures are real frames from docs/captures/20261118-190530_ps3_separate.json.
 * Build and run: powershell -File tests/run.ps1
 */

#include <stdio.h>
#include <string.h>

#include "../src/report.h"

typedef struct {
    const char *name;
    uint8_t frame[VOLANTE_REPORT_LEN];
    uint8_t steering;
    uint8_t brake;
    uint8_t throttle;
    uint8_t hat;
    uint16_t buttons;
} fixture;

/* clang-format off */
static const fixture fixtures[] = {
    { "baseline",    { 0x00, 0xE0, 0xFF, 0x43, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0x43, 0xFF, 0xFF, VOLANTE_HAT_NONE, 0 },
    { "steer_left",  { 0x00, 0xE0, 0xFF, 0x00, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0x00, 0xFF, 0xFF, VOLANTE_HAT_NONE, 0 },
    { "steer_right", { 0x00, 0xE0, 0xFF, 0xFF, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0xFF, 0xFF, 0xFF, VOLANTE_HAT_NONE, 0 },
    { "throttle",    { 0x00, 0xE0, 0xFF, 0x80, 0xFF, 0x00, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0x80, 0xFF, 0x00, VOLANTE_HAT_NONE, 0 },
    { "brake",       { 0x00, 0xE0, 0xFF, 0x80, 0x00, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0x80, 0x00, 0xFF, VOLANTE_HAT_NONE, 0 },
    { "button_1",    { 0x01, 0xE0, 0xFF, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0x80, 0xFF, 0xFF, VOLANTE_HAT_NONE, VOLANTE_BTN_1 },
    { "button_2",    { 0x02, 0xE0, 0xFF, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0x80, 0xFF, 0xFF, VOLANTE_HAT_NONE, VOLANTE_BTN_2 },
    { "button_3",    { 0x04, 0xE0, 0xFF, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0x80, 0xFF, 0xFF, VOLANTE_HAT_NONE, VOLANTE_BTN_3 },
    { "button_4",    { 0x08, 0xE0, 0xFF, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0x80, 0xFF, 0xFF, VOLANTE_HAT_NONE, VOLANTE_BTN_4 },
    { "paddle_up",   { 0x20, 0xE0, 0xFF, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0x80, 0xFF, 0xFF, VOLANTE_HAT_NONE, VOLANTE_BTN_PADDLE_UP },
    { "paddle_down", { 0x10, 0xE0, 0xFF, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0x80, 0xFF, 0xFF, VOLANTE_HAT_NONE, VOLANTE_BTN_PADDLE_DOWN },
    { "dpad_up",     { 0x00, 0xE0, 0x00, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0x80, 0xFF, 0xFF, VOLANTE_HAT_UP, 0 },
    { "dpad_right",  { 0x00, 0xE0, 0x02, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0x80, 0xFF, 0xFF, VOLANTE_HAT_RIGHT, 0 },
    { "dpad_down",   { 0x00, 0xE0, 0x04, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0x80, 0xFF, 0xFF, VOLANTE_HAT_DOWN, 0 },
    { "dpad_left",   { 0x00, 0xE0, 0x06, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0x80, 0xFF, 0xFF, VOLANTE_HAT_LEFT, 0 },
    { "se",          { 0x00, 0xE1, 0xFF, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0x80, 0xFF, 0xFF, VOLANTE_HAT_NONE, VOLANTE_BTN_SE },
    { "st",          { 0x00, 0xE2, 0xFF, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0x80, 0xFF, 0xFF, VOLANTE_HAT_NONE, VOLANTE_BTN_ST },
    { "home",        { 0x00, 0xF0, 0xFF, 0x80, 0xFF, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0x80, 0xFF, 0xFF, VOLANTE_HAT_NONE, VOLANTE_BTN_HOME },
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
        volante_report report;

        memset(&report, 0, sizeof(report));
        if (volante_parse(item->frame, VOLANTE_REPORT_LEN, &report) != 0) {
            printf("  FAIL  %s: parser rejected a valid frame\n", item->name);
            failures++;
            continue;
        }
        check(item->name, "steering", report.steering, item->steering);
        check(item->name, "brake", report.brake, item->brake);
        check(item->name, "throttle", report.throttle, item->throttle);
        check(item->name, "hat", report.hat, item->hat);
        check(item->name, "buttons", report.buttons, item->buttons);
    }

    {
        uint8_t short_frame[VOLANTE_REPORT_LEN] = { 0 };
        volante_report report;

        if (volante_parse(short_frame, VOLANTE_REPORT_LEN - 1, &report) != -1) {
            printf("  FAIL  short frame accepted\n");
            failures++;
        }
        if (volante_parse(NULL, VOLANTE_REPORT_LEN, &report) != -1) {
            printf("  FAIL  null buffer accepted\n");
            failures++;
        }
    }

    if (failures == 0) {
        printf("ok: %d fixtures, 0 failures\n", (int)count);
        return 0;
    }
    printf("%d failure(s) across %d fixtures\n", failures, (int)count);
    return 1;
}
