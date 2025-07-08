#ifndef __HID_PROX_DATA_H__
#define __HID_PROX_DATA_H__

#include "data_utils.h"
#include "bsp_time.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define HID_PROX_CARD_ID_SIZE 3     // HID Prox card ID size (24 bits)
#define HID_PROX_FACILITY_SIZE 1    // HID Prox facility code size (8 bits)
#define HID_PROX_TOTAL_SIZE 4       // Total data size (32 bits)
#define LF_HIDPROX_TAG_ID_SIZE 4    // For compatibility with command interface

#define HID_PROX_RAW_BUF_SIZE 24    // Raw buffer size for detection
#define HID_PROX_RAW_BITS 128       // Maximum raw bits to capture
#define HID_PROX_BIT_PERIOD_RF_8 8  // 8 RF cycles per bit period
#define HID_PROX_BIT_PERIOD_RF_10 10 // 10 RF cycles per bit period

typedef struct {
    uint8_t facility_code;          // Facility code (8 bits)
    uint16_t card_number;           // Card number (16 bits)
    uint8_t padding;                // Padding to make structure 4 bytes total
} hid_prox_card_data_t;

typedef struct {
    uint8_t raw_data[HID_PROX_RAW_BUF_SIZE];  // Raw captured data
    uint8_t timing_data[HID_PROX_RAW_BITS];   // Timing data for FSK decoding
    uint8_t decoded_data[HID_PROX_TOTAL_SIZE]; // Decoded HID Prox data
    uint8_t start_bit_pos;                     // Starting bit position
    uint8_t data_valid;                        // Data validity flag
} hid_prox_raw_data_t;

void init_hidprox_hw(void);
uint8_t hidprox_read(hid_prox_card_data_t *card_data, uint32_t timeout_ms);
uint8_t hidprox_encode(hid_prox_card_data_t *card_data, uint8_t *output_buffer);
uint8_t hidprox_decode(uint8_t *raw_data, uint8_t size, hid_prox_card_data_t *card_data);
uint8_t hidprox_acquire(void);
void GPIO_hidprox_callback(void);

#ifdef __cplusplus
}
#endif

#endif