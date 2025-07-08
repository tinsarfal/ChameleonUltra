#include "bsp_time.h"
#include "bsp_delay.h"
#include "lf_reader_data.h"
#include "lf_hidprox_data.h"
#include "lf_125khz_radio.h"

#define NRF_LOG_MODULE_NAME hidprox
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();

static hid_prox_raw_data_t hidprox_raw_data;
static volatile uint8_t data_index = 0;
static uint8_t hidprox_card_buffer[HID_PROX_TOTAL_SIZE];
static volatile bool hidprox_card_found = false;

/**
 * Process HID Prox FSK signal and decode 26-bit data
 */
uint8_t hidprox_acquire(void) {
    if (data_index >= HID_PROX_RAW_BITS) {
        // Look for HID Prox FSK pattern
        // HID Prox uses different timing than EM410x - typically longer periods for FSK
        uint32_t bit_data = 0;
        uint8_t valid_bits = 0;
        
        // Decode FSK timing data into bits
        for (int i = 0; i < data_index && valid_bits < 26; i++) {
            uint8_t timing_value = hidprox_raw_data.timing_data[i];
            
            // HID Prox FSK timing analysis
            // Adjust these thresholds based on actual HID Prox timing characteristics
            if (timing_value >= 32 && timing_value <= 48) {
                // Short period = 0 bit
                bit_data <<= 1;
                valid_bits++;
            } else if (timing_value >= 64 && timing_value <= 80) {
                // Long period = 1 bit  
                bit_data = (bit_data << 1) | 1;
                valid_bits++;
            }
            // Invalid timing values are ignored
        }
        
        // We need exactly 26 bits for HID Prox
        if (valid_bits == 26) {
            // Store the decoded data
            hidprox_card_buffer[0] = (bit_data >> 24) & 0xFF;
            hidprox_card_buffer[1] = (bit_data >> 16) & 0xFF;
            hidprox_card_buffer[2] = (bit_data >> 8) & 0xFF;
            hidprox_card_buffer[3] = (bit_data >> 0) & 0xFF;
            
            hidprox_card_found = true;
            data_index = 0;
            return 1;
        }
        
        // Reset for next attempt
        data_index = 0;
    }
    return 0;
}

/**
 * HID Prox GPIO interrupt callback for FSK signal processing
 */
void GPIO_hidprox_callback(void) {
    static uint32_t this_time_len = 0;
    this_time_len = get_lf_counter_value();
    
    if (this_time_len > 20) { // Filter out noise
        if (data_index < HID_PROX_RAW_BITS) {
            // Store timing information for FSK analysis
            hidprox_raw_data.timing_data[data_index] = (uint8_t)(this_time_len & 0xFF);
            data_index++;
        }
        clear_lf_counter_value();
    }
    
    // Small delay for hardware stability
    uint16_t counter = 0;
    do {
        __NOP();
    } while (counter++ > 500);
}

/**
 * Initialize HID Prox hardware
 */
void init_hidprox_hw(void) {
    // Initialize the LF radio for 125kHz operations
    lf_125khz_radio_init();
    // Register HID Prox-specific GPIO callback for FSK processing
    register_rio_callback(GPIO_hidprox_callback);
    NRF_LOG_INFO("HID Prox hardware initialized");
}

/**
 * Calculate parity for HID Prox format
 * HID Prox uses even parity for the first 12 bits and odd parity for the last 12 bits
 */
static uint8_t hidprox_calc_parity(uint32_t data, uint8_t start_bit, uint8_t length, uint8_t parity_type) {
    uint8_t parity = 0;
    for (uint8_t i = 0; i < length; i++) {
        if ((data >> (start_bit + i)) & 1) {
            parity ^= 1;
        }
    }
    // For odd parity, invert the result
    if (parity_type == 1) {
        parity ^= 1;
    }
    return parity;
}

/**
 * Encode HID Prox card data into FSK format
 */
uint8_t hidprox_encode(hid_prox_card_data_t *card_data, uint8_t *output_buffer) {
    if (!card_data || !output_buffer) {
        return 0;
    }

    // HID Prox format: 26-bit Wiegand format
    // Bit layout: P0 + 8-bit facility + 16-bit card number + P1
    uint32_t wiegand_data = 0;
    
    // Set facility code (bits 1-8)
    wiegand_data |= ((uint32_t)card_data->facility_code & 0xFF) << 17;
    
    // Set card number (bits 9-24)
    wiegand_data |= ((uint32_t)card_data->card_number & 0xFFFF) << 1;
    
    // Calculate even parity for first 12 bits (P0)
    uint8_t p0 = hidprox_calc_parity(wiegand_data, 1, 12, 0);
    wiegand_data |= ((uint32_t)p0) << 25;
    
    // Calculate odd parity for last 12 bits (P1)
    uint8_t p1 = hidprox_calc_parity(wiegand_data, 13, 12, 1);
    wiegand_data |= ((uint32_t)p1);
    
    // Convert to FSK format
    // For now, store as 32-bit little-endian
    output_buffer[0] = (wiegand_data >> 0) & 0xFF;
    output_buffer[1] = (wiegand_data >> 8) & 0xFF;
    output_buffer[2] = (wiegand_data >> 16) & 0xFF;
    output_buffer[3] = (wiegand_data >> 24) & 0xFF;
    
    return HID_PROX_TOTAL_SIZE;
}

/**
 * Decode raw HID Prox data
 */
uint8_t hidprox_decode(uint8_t *raw_data, uint8_t size, hid_prox_card_data_t *card_data) {
    if (!raw_data || !card_data || size < HID_PROX_TOTAL_SIZE) {
        return 0;
    }

    // Reconstruct 32-bit data
    uint32_t wiegand_data = 0;
    wiegand_data |= ((uint32_t)raw_data[0]) << 0;
    wiegand_data |= ((uint32_t)raw_data[1]) << 8;
    wiegand_data |= ((uint32_t)raw_data[2]) << 16;
    wiegand_data |= ((uint32_t)raw_data[3]) << 24;
    
    // Extract parity bits
    uint8_t p0 = (wiegand_data >> 25) & 1;
    uint8_t p1 = (wiegand_data >> 0) & 1;
    
    // Extract facility code (bits 1-8)
    card_data->facility_code = (wiegand_data >> 17) & 0xFF;
    
    // Extract card number (bits 9-24)
    card_data->card_number = (wiegand_data >> 1) & 0xFFFF;
    
    // Verify parity
    uint8_t calc_p0 = hidprox_calc_parity(wiegand_data, 1, 12, 0);
    uint8_t calc_p1 = hidprox_calc_parity(wiegand_data, 13, 12, 1);
    
    if (calc_p0 != p0 || calc_p1 != p1) {
        NRF_LOG_WARNING("HID Prox parity check failed");
        return 0;
    }
    
    return 1;
}

/**
 * Read HID Prox card
 */
uint8_t hidprox_read(hid_prox_card_data_t *card_data, uint32_t timeout_ms) {
    if (!card_data) {
        return 0;
    }

    uint8_t result = 0;
    
    // Initialize raw data structure
    memset(&hidprox_raw_data, 0, sizeof(hidprox_raw_data));
    hidprox_card_found = false;
    data_index = 0;
    
    NRF_LOG_INFO("Starting HID Prox read, timeout: %d ms", timeout_ms);
    
    init_hidprox_hw();          // Initialize HID Prox hardware with FSK callback
    start_lf_125khz_radio();    // Start 125kHz carrier
    
    // Reading the card during timeout
    autotimer *p_at = bsp_obtain_timer(0);
    while (NO_TIMEOUT_1MS(p_at, timeout_ms)) {
        // Process captured FSK signal data
        if (hidprox_acquire()) {
            // Successfully decoded HID Prox data
            result = hidprox_decode(hidprox_card_buffer, HID_PROX_TOTAL_SIZE, card_data);
            if (result) {
                break;
            }
        }
        
        bsp_delay_ms(10);  // Small delay between processing attempts
    }
    
    // Clean up
    stop_lf_125khz_radio();
    unregister_rio_callback();  // Unregister the callback to avoid interference
    
    bsp_return_timer(p_at);
    p_at = NULL;
    
    if (result) {
        NRF_LOG_INFO("HID Prox card detected: Facility=%d, Card=%d", 
                     card_data->facility_code, card_data->card_number);
    } else {
        NRF_LOG_INFO("No HID Prox card detected");
    }
    
    return result;
}