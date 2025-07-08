#include "lf_tag_hidprox.h"
#include "tag_persistence.h"
#include "tag_emulation.h"
#include "hex_utils.h"
#include "bsp_time.h"
#include "bsp_delay.h"
#include "lf_125khz_radio.h"

#define NRF_LOG_MODULE_NAME lf_tag_hidprox
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();

// Static data for current HID Prox tag
static lf_tag_hidprox_info_t m_tag_info = {0};

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
 * Initialize HID Prox simulation
 */
void lf_tag_hidprox_simulation_init(void) {
    // Initialize LF hardware for emulation
    lf_125khz_radio_init();
    m_tag_info.emulation_enabled = true;
    
    NRF_LOG_INFO("HID Prox simulation initialized");
}

/**
 * Deinitialize HID Prox simulation
 */
void lf_tag_hidprox_simulation_deinit(void) {
    m_tag_info.emulation_enabled = false;
    
    NRF_LOG_INFO("HID Prox simulation deinitialized");
}

/**
 * Process HID Prox simulation (called in main loop)
 */
void lf_tag_hidprox_simulation_process(void) {
    if (!m_tag_info.emulation_enabled) {
        return;
    }
    
    // In a real implementation, this would:
    // 1. Monitor for LF carrier from reader
    // 2. Respond with FSK modulated HID Prox data
    // 3. Handle timing requirements for HID Prox protocol
    
    // For now, this is a placeholder that indicates the tag is ready
    // The actual FSK transmission would require precise timing control
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