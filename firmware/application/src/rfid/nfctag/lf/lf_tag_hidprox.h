#ifndef LF_TAG_HIDPROX_H
#define LF_TAG_HIDPROX_H

#include <stdint.h>
#include <stdbool.h>
#include "tag_base_type.h"
#include "tag_emulation.h"
#include "rfid/reader/lf/lf_hidprox_data.h"

#ifdef __cplusplus
extern "C" {
#endif

// HID Prox emulation timing constants
#define HID_PROX_BIT_PERIOD_US 64              // Bit period in microseconds for HID Prox
#define HID_PROX_MANCHESTER_HALF_PERIOD_US 32  // Half bit period for Manchester encoding
#define HID_PROX_TRANSMISSION_INTERVAL_MS 100  // Interval between transmissions
#define HID_PROX_PREAMBLE_BITS 5               // Number of preamble bits
#define HID_PROX_TOTAL_BITS 31                 // Total bits including preamble

// HID Prox specific tag data
typedef struct {
    hid_prox_card_data_t card_data;
    uint8_t emulation_enabled;
    uint8_t transmission_buffer[HID_PROX_TOTAL_BITS];  // Buffer for Manchester encoded transmission
    uint8_t transmission_bit_position;                  // Current bit position in transmission
    uint8_t transmission_phase;                         // Manchester phase (0 = first half, 1 = second half)
    uint8_t transmission_active;                        // Whether transmission is currently active
} lf_tag_hidprox_info_t;

// Function prototypes
int lf_tag_hidprox_data_loadcb(tag_specific_type_t type, tag_data_buffer_t* buffer);
int lf_tag_hidprox_data_savecb(tag_specific_type_t type, tag_data_buffer_t* buffer);
bool lf_tag_hidprox_data_factory(uint8_t slot, tag_specific_type_t tag_type);
void lf_tag_hidprox_simulation_init(void);
void lf_tag_hidprox_simulation_deinit(void);
void lf_tag_hidprox_simulation_process(void);

// Get/Set functions for HID Prox data
hid_prox_card_data_t* lf_tag_hidprox_get_card_data(void);
void lf_tag_hidprox_set_card_data(hid_prox_card_data_t* card_data);
bool lf_tag_hidprox_is_emulation_enabled(void);
void lf_tag_hidprox_set_emulation_enabled(bool enabled);

#ifdef __cplusplus
}
#endif

#endif