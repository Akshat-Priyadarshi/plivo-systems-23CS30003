#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// The raw format expected by the harness (164 bytes)
struct HarnessPacket {
    uint32_t seq;
    uint8_t payload[160];
} __attribute__((packed));

// Your custom wire format sent across the flaky network (324 bytes)
struct FecPacket {
    uint32_t seq;
    uint8_t current_payload[160];
    uint8_t prev_payload[160]; // Allows recovery if seq-1 was dropped
} __attribute__((packed));

#endif