/*
 * Wire format between the laptop bridge and the PS4 plugin.
 *
 * The bridge sends the raw 19-byte HID report unchanged. All parsing and
 * mapping stays in C, in one place, so there is no second implementation to
 * keep in sync.
 *
 * Layout, little endian:
 *
 *   offset  size  field
 *   0       4     magic, 'V' 'L' 'N' 'T'
 *   4       2     sequence number, wraps
 *   6       1     report length, 19
 *   7       1     flags, 0
 *   8       19    raw input report, see docs/REPORTS.md
 *
 * 27 bytes. Fits one UDP datagram with no fragmentation.
 */

#ifndef VOLANTE_WIRE_H
#define VOLANTE_WIRE_H

#include <stdint.h>

#include "report.h"

#define VOLANTE_WIRE_MAGIC0 'V'
#define VOLANTE_WIRE_MAGIC1 'L'
#define VOLANTE_WIRE_MAGIC2 'N'
#define VOLANTE_WIRE_MAGIC3 'T'

#define VOLANTE_WIRE_HEADER 8u
#define VOLANTE_WIRE_LEN (VOLANTE_WIRE_HEADER + VOLANTE_REPORT_LEN)

#define VOLANTE_WIRE_PORT 9107

/*
 * A packet is accepted when the magic matches and the length field equals
 * VOLANTE_REPORT_LEN. Magic check is first so a stray datagram is cheap to
 * reject.
 */
static inline int volante_wire_valid(const uint8_t *data, unsigned len)
{
    if (len != VOLANTE_WIRE_LEN) {
        return 0;
    }
    return data[0] == VOLANTE_WIRE_MAGIC0 && data[1] == VOLANTE_WIRE_MAGIC1 &&
           data[2] == VOLANTE_WIRE_MAGIC2 && data[3] == VOLANTE_WIRE_MAGIC3 &&
           data[6] == VOLANTE_REPORT_LEN;
}

#endif /* VOLANTE_WIRE_H */
