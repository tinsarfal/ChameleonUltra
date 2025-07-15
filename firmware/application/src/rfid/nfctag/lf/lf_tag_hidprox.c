#include "lf_tag_hidprox.h"
#include "tag_persistence.h"
#include "tag_emulation.h"
#include "hex_utils.h"
#include "bsp_time.h"
#include "bsp_delay.h"
#include "lf_125khz_radio.h"
#include "hw_connect.h"
#include "syssleep.h"
#include "lf_tag_em.h"

#include "nrf_drv_timer.h"
#include "nrf_drv_lpcomp.h"
#include "nrf_gpio.h"

#define NRF_LOG_MODULE_NAME lf_tag_hidprox
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();

// Static data for current HID Prox tag
static lf_tag_hidprox_info_t m_tag_info = {0};

// Timer for HID Prox transmission timing
static const nrfx_timer_t m_timer_hidprox = NRFX_TIMER_INSTANCE(1);

// Field detection state
static volatile bool m_is_lf_emulating = false;
static volatile bool m_field_detected = false;

// External variables
extern bool g_is_tag_emulating;
extern bool g_usb_led_marquee_enable;

// Function prototypes
static void hidprox_encode_manchester(void);
static void hidprox_timer_handler(nrf_timer_event_t event_type, void* p_context);
static void hidprox_field_handler(nrf_lpcomp_event_t event);
static void hidprox_modulation_control(bool enable);

/**
 * Load HID Prox tag data from buffer
 */
int lf_tag_hidprox_data_loadcb(tag_specific_type_t type, tag_data_buffer_t* buffer) {
    if (type != TAG_TYPE_HID_PROX || buffer == NULL || buffer->length < LF_HIDPROX_TAG_ID_SIZE) {
        return 1; // Error
    }
    
    memcpy(&m_tag_info.card_data, buffer->buffer, LF_HIDPROX_TAG_ID_SIZE);
    m_tag_info.emulation_enabled = true;
    
    NRF_LOG_INFO("HID Prox tag data loaded: Facility=%d, Card=%d", 
                 m_tag_info.card_data.facility_code, 
                 m_tag_info.card_data.card_number);
    
    return 0; // Success
}

/**
 * Save HID Prox tag data to buffer
 */
int lf_tag_hidprox_data_savecb(tag_specific_type_t type, tag_data_buffer_t* buffer) {
    if (type != TAG_TYPE_HID_PROX || buffer == NULL) {
        return 1; // Error
    }
    
    if (buffer->length < LF_HIDPROX_TAG_ID_SIZE) {
        return 1; // Error
    }
    
    memcpy(buffer->buffer, &m_tag_info.card_data, LF_HIDPROX_TAG_ID_SIZE);
    buffer->length = LF_HIDPROX_TAG_ID_SIZE;
    
    return 0; // Success
}

/**
 * Create factory default HID Prox tag data
 */
bool lf_tag_hidprox_data_factory(uint8_t slot, tag_specific_type_t tag_type) {
    if (tag_type != TAG_TYPE_HID_PROX) {
        return false;
    }
    
    // Create default HID Prox data
    m_tag_info.card_data.facility_code = 0;  // Default facility code
    m_tag_info.card_data.card_number = 1234 + slot;  // Default card number based on slot
    m_tag_info.emulation_enabled = true;
    
    NRF_LOG_INFO("HID Prox factory data created for slot %d: Facility=%d, Card=%d", 
                 slot,
                 m_tag_info.card_data.facility_code, 
                 m_tag_info.card_data.card_number);
    
    return true;
}

/**
 * Initialize HID Prox simulation with proper timing and field detection
 */
void lf_tag_hidprox_simulation_init(void) {
    ret_code_t err_code;
    
    // Initialize LF hardware for emulation
    lf_125khz_radio_init();
    
    // Configure GPIO for modulation control
    nrf_gpio_cfg_output(LF_MOD);
    nrf_gpio_pin_clear(LF_MOD);
    
    // Prepare Manchester encoded data for transmission
    hidprox_encode_manchester();
    
    // Initialize timer for bit transmission
    nrfx_timer_config_t timer_cfg = NRFX_TIMER_DEFAULT_CONFIG;
    timer_cfg.frequency = NRFX_TIMER_BASE_FREQUENCY_16MHz;
    timer_cfg.mode = NRF_TIMER_MODE_TIMER;
    timer_cfg.bit_width = NRF_TIMER_BIT_WIDTH_32;
    
    err_code = nrfx_timer_init(&m_timer_hidprox, &timer_cfg, hidprox_timer_handler);
    APP_ERROR_CHECK(err_code);
    
    // Set timer to trigger every half-bit period (32 microseconds)
    nrfx_timer_extended_compare(&m_timer_hidprox, 
                               NRF_TIMER_CC_CHANNEL2, 
                               nrfx_timer_us_to_ticks(&m_timer_hidprox, HID_PROX_MANCHESTER_HALF_PERIOD_US),
                               NRF_TIMER_SHORT_COMPARE2_CLEAR_MASK, 
                               true);
    
    // Configure field detection using LPCOMP
    nrf_drv_lpcomp_config_t lpcomp_config = NRF_DRV_LPCOMP_DEFAULT_CONFIG;
    lpcomp_config.hal.reference = NRF_LPCOMP_REF_SUPPLY_1_16;
    lpcomp_config.input = LF_RSSI;
    lpcomp_config.hal.detection = NRF_LPCOMP_DETECT_UP;
    lpcomp_config.hal.hyst = NRF_LPCOMP_HYST_50mV;
    
    err_code = nrf_drv_lpcomp_init(&lpcomp_config, hidprox_field_handler);
    APP_ERROR_CHECK(err_code);
    
    // Initialize state variables
    m_tag_info.emulation_enabled = true;
    m_tag_info.transmission_bit_position = 0;
    m_tag_info.transmission_phase = 0;
    m_tag_info.transmission_active = false;
    m_is_lf_emulating = false;
    m_field_detected = false;
    
    NRF_LOG_INFO("HID Prox simulation initialized with FSK modulation");
}

/**
 * Deinitialize HID Prox simulation
 */
void lf_tag_hidprox_simulation_deinit(void) {
    // Stop timer
    nrfx_timer_uninit(&m_timer_hidprox);
    
    // Deinitialize LPCOMP
    nrf_drv_lpcomp_uninit();
    
    // Turn off modulation
    nrf_gpio_pin_clear(LF_MOD);
    
    // Reset state
    m_tag_info.emulation_enabled = false;
    m_tag_info.transmission_active = false;
    m_is_lf_emulating = false;
    m_field_detected = false;
    
    // Turn off field LED
    TAG_FIELD_LED_OFF();
    
    NRF_LOG_INFO("HID Prox simulation deinitialized");
}

/**
 * Process HID Prox simulation (called in main loop)
 */
void lf_tag_hidprox_simulation_process(void) {
    if (!m_tag_info.emulation_enabled) {
        return;
    }
    
    // Check if field is still present
    if (m_is_lf_emulating && !lf_is_field_exists()) {
        // Field lost - stop emulation
        g_is_tag_emulating = false;
        m_is_lf_emulating = false;
        m_field_detected = false;
        m_tag_info.transmission_active = false;
        
        nrfx_timer_disable(&m_timer_hidprox);
        hidprox_modulation_control(false);
        
        TAG_FIELD_LED_OFF();
        
        // Re-enable LPCOMP for field detection
        nrf_drv_lpcomp_enable();
        
        // Start sleep timer
        sleep_timer_start(SLEEP_DELAY_MS_FIELD_125KHZ_LOST);
        
        NRF_LOG_INFO("HID Prox field lost - stopping emulation");
    }
    
    // Check if we need to re-encode data (in case card data changed)
    static uint32_t last_card_data = 0;
    uint32_t current_card_data = (((uint32_t)m_tag_info.card_data.facility_code) << 16) | 
                                 m_tag_info.card_data.card_number;
    
    if (current_card_data != last_card_data) {
        hidprox_encode_manchester();
        last_card_data = current_card_data;
    }
}

/**
 * Encode HID Prox data into Manchester format for transmission
 */
static void hidprox_encode_manchester(void) {
    uint8_t encoded_buffer[HID_PROX_TOTAL_SIZE];
    uint8_t buffer_size = hidprox_encode(&m_tag_info.card_data, encoded_buffer);
    
    if (buffer_size != HID_PROX_TOTAL_SIZE) {
        NRF_LOG_ERROR("Failed to encode HID Prox data");
        return;
    }
    
    // Reconstruct the 26-bit Wiegand data
    uint32_t wiegand_data = 0;
    wiegand_data |= ((uint32_t)encoded_buffer[0]) << 0;
    wiegand_data |= ((uint32_t)encoded_buffer[1]) << 8;
    wiegand_data |= ((uint32_t)encoded_buffer[2]) << 16;
    wiegand_data |= ((uint32_t)encoded_buffer[3]) << 24;
    
    // Extract only the 26 bits we need
    wiegand_data &= 0x03FFFFFF;
    
    // Clear transmission buffer
    memset(m_tag_info.transmission_buffer, 0, sizeof(m_tag_info.transmission_buffer));
    
    // Add preamble (5 bits of 1s)
    uint8_t bit_pos = 0;
    for (int i = 0; i < HID_PROX_PREAMBLE_BITS; i++) {
        m_tag_info.transmission_buffer[bit_pos++] = 1;
    }
    
    // Add the 26-bit Wiegand data (MSB first)
    for (int i = 25; i >= 0; i--) {
        m_tag_info.transmission_buffer[bit_pos++] = (wiegand_data >> i) & 1;
    }
    
    NRF_LOG_INFO("HID Prox Manchester encoded: %d bits, wiegand: 0x%08X", 
                 bit_pos, wiegand_data);
}

/**
 * FSK modulation control for HID Prox transmission
 */
static void hidprox_modulation_control(bool enable) {
    if (enable) {
        // Enable FSK modulation (pull LF_MOD high)
        nrf_gpio_pin_set(LF_MOD);
    } else {
        // Disable FSK modulation (pull LF_MOD low)
        nrf_gpio_pin_clear(LF_MOD);
    }
}

/**
 * Timer handler for HID Prox bit transmission
 */
static void hidprox_timer_handler(nrf_timer_event_t event_type, void* p_context) {
    if (event_type == NRF_TIMER_EVENT_COMPARE2) {
        if (!m_is_lf_emulating || !m_tag_info.transmission_active) {
            return;
        }
        
        // Get current bit to transmit
        uint8_t current_bit = m_tag_info.transmission_buffer[m_tag_info.transmission_bit_position];
        
        // Manchester encoding: 1 = high-to-low, 0 = low-to-high
        if (m_tag_info.transmission_phase == 0) {
            // First half of bit period
            if (current_bit == 1) {
                hidprox_modulation_control(true);   // High for first half of '1'
            } else {
                hidprox_modulation_control(false);  // Low for first half of '0'
            }
            m_tag_info.transmission_phase = 1;
        } else {
            // Second half of bit period
            if (current_bit == 1) {
                hidprox_modulation_control(false);  // Low for second half of '1'
            } else {
                hidprox_modulation_control(true);   // High for second half of '0'
            }
            m_tag_info.transmission_phase = 0;
            m_tag_info.transmission_bit_position++;
            
            // Check if we've transmitted all bits
            if (m_tag_info.transmission_bit_position >= HID_PROX_TOTAL_BITS) {
                m_tag_info.transmission_bit_position = 0;
                m_tag_info.transmission_active = false;
                hidprox_modulation_control(false);  // Turn off modulation
                
                // Schedule next transmission
                bsp_delay_ms(HID_PROX_TRANSMISSION_INTERVAL_MS);
                if (m_is_lf_emulating) {
                    m_tag_info.transmission_active = true;
                }
            }
        }
    }
}

/**
 * Field detection handler for HID Prox emulation
 */
static void hidprox_field_handler(nrf_lpcomp_event_t event) {
    if (!m_is_lf_emulating && event == NRF_LPCOMP_EVENT_UP) {
        // Field detected - start emulation
        sleep_timer_stop();
        nrf_drv_lpcomp_disable();
        
        m_is_lf_emulating = true;
        g_is_tag_emulating = true;
        m_field_detected = true;
        
        // Disable USB LED effects during emulation
        g_usb_led_marquee_enable = false;
        
        // Set LED to indicate HID Prox emulation
        set_slot_light_color(RGB_CYAN);
        TAG_FIELD_LED_ON();
        
        // Reset transmission state
        m_tag_info.transmission_bit_position = 0;
        m_tag_info.transmission_phase = 0;
        m_tag_info.transmission_active = true;
        
        // Enable timer for transmission
        nrfx_timer_enable(&m_timer_hidprox);
        
        NRF_LOG_INFO("HID Prox field detected - starting emulation");
    }
}

/**
 * Get current HID Prox card data
 */
hid_prox_card_data_t* lf_tag_hidprox_get_card_data(void) {
    return &m_tag_info.card_data;
}

/**
 * Set HID Prox card data
 */
void lf_tag_hidprox_set_card_data(hid_prox_card_data_t* card_data) {
    if (card_data) {
        memcpy(&m_tag_info.card_data, card_data, LF_HIDPROX_TAG_ID_SIZE);
        NRF_LOG_INFO("HID Prox card data updated: Facility=%d, Card=%d", 
                     m_tag_info.card_data.facility_code, 
                     m_tag_info.card_data.card_number);
    }
}

/**
 * Check if emulation is enabled
 */
bool lf_tag_hidprox_is_emulation_enabled(void) {
    return m_tag_info.emulation_enabled;
}

/**
 * Enable/disable emulation
 */
void lf_tag_hidprox_set_emulation_enabled(bool enabled) {
    m_tag_info.emulation_enabled = enabled;
    
    if (enabled) {
        lf_tag_hidprox_simulation_init();
    } else {
        lf_tag_hidprox_simulation_deinit();
    }
}