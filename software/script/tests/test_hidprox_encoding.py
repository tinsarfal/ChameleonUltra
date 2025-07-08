#!/usr/bin/env python3

import unittest


class TestHIDProxEncoding(unittest.TestCase):
    """Test HID Prox encoding/decoding logic (Python implementation for validation)"""
    
    def hidprox_calc_parity(self, data, start_bit, length, parity_type):
        """Python implementation of HID Prox parity calculation"""
        parity = 0
        for i in range(length):
            if (data >> (start_bit + i)) & 1:
                parity ^= 1
        # For odd parity, invert the result
        if parity_type == 1:
            parity ^= 1
        return parity
    
    def hidprox_encode(self, facility_code, card_number):
        """Python implementation of HID Prox encoding"""
        # HID Prox format: 26-bit Wiegand format
        # Bit layout: P0 + 8-bit facility + 16-bit card number + P1
        wiegand_data = 0
        
        # Set facility code (bits 1-8)
        wiegand_data |= (facility_code & 0xFF) << 17
        
        # Set card number (bits 9-24)
        wiegand_data |= (card_number & 0xFFFF) << 1
        
        # Calculate even parity for first 12 bits (P0)
        p0 = self.hidprox_calc_parity(wiegand_data, 1, 12, 0)
        wiegand_data |= p0 << 25
        
        # Calculate odd parity for last 12 bits (P1)
        p1 = self.hidprox_calc_parity(wiegand_data, 13, 12, 1)
        wiegand_data |= p1
        
        # Convert to bytes (little-endian)
        output_buffer = []
        output_buffer.append((wiegand_data >> 0) & 0xFF)
        output_buffer.append((wiegand_data >> 8) & 0xFF)
        output_buffer.append((wiegand_data >> 16) & 0xFF)
        output_buffer.append((wiegand_data >> 24) & 0xFF)
        
        return bytes(output_buffer)
    
    def hidprox_decode(self, raw_data):
        """Python implementation of HID Prox decoding"""
        if len(raw_data) < 4:
            return None
            
        # Reconstruct 32-bit data
        wiegand_data = 0
        wiegand_data |= raw_data[0] << 0
        wiegand_data |= raw_data[1] << 8
        wiegand_data |= raw_data[2] << 16
        wiegand_data |= raw_data[3] << 24
        
        # Extract parity bits
        p0 = (wiegand_data >> 25) & 1
        p1 = (wiegand_data >> 0) & 1
        
        # Extract facility code (bits 1-8)
        facility_code = (wiegand_data >> 17) & 0xFF
        
        # Extract card number (bits 9-24)
        card_number = (wiegand_data >> 1) & 0xFFFF
        
        # Verify parity
        calc_p0 = self.hidprox_calc_parity(wiegand_data, 1, 12, 0)
        calc_p1 = self.hidprox_calc_parity(wiegand_data, 13, 12, 1)
        
        if calc_p0 != p0 or calc_p1 != p1:
            return None  # Parity check failed
            
        return {
            'facility_code': facility_code,
            'card_number': card_number
        }
    
    def test_encode_decode_roundtrip(self):
        """Test that encoding and then decoding returns the original values"""
        test_cases = [
            (0, 0),           # Minimum values
            (123, 45678),     # Example values from firmware
            (255, 65535),     # Maximum values
            (128, 32768),     # Mid-range values
            (1, 1),           # Small values
        ]
        
        for facility_code, card_number in test_cases:
            with self.subTest(facility=facility_code, card=card_number):
                # Encode
                encoded = self.hidprox_encode(facility_code, card_number)
                self.assertEqual(len(encoded), 4)
                
                # Decode
                decoded = self.hidprox_decode(encoded)
                self.assertIsNotNone(decoded)
                self.assertEqual(decoded['facility_code'], facility_code)
                self.assertEqual(decoded['card_number'], card_number)
    
    def test_parity_calculation(self):
        """Test parity calculation for known values"""
        # Test even parity
        self.assertEqual(self.hidprox_calc_parity(0b1111, 0, 4, 0), 0)  # Even number of 1s
        self.assertEqual(self.hidprox_calc_parity(0b1110, 0, 4, 0), 1)  # Odd number of 1s
        
        # Test odd parity 
        self.assertEqual(self.hidprox_calc_parity(0b1111, 0, 4, 1), 1)  # Even number of 1s, inverted
        self.assertEqual(self.hidprox_calc_parity(0b1110, 0, 4, 1), 0)  # Odd number of 1s, inverted
    
    def test_wiegand_26_bit_format(self):
        """Test specific 26-bit Wiegand format requirements"""
        facility_code = 123
        card_number = 45678
        
        encoded = self.hidprox_encode(facility_code, card_number)
        
        # Reconstruct the full 32-bit value
        wiegand_data = 0
        wiegand_data |= encoded[0] << 0
        wiegand_data |= encoded[1] << 8
        wiegand_data |= encoded[2] << 16
        wiegand_data |= encoded[3] << 24
        
        # Check that the facility code is in the right position (bits 17-24)
        extracted_facility = (wiegand_data >> 17) & 0xFF
        self.assertEqual(extracted_facility, facility_code)
        
        # Check that the card number is in the right position (bits 1-16)
        extracted_card = (wiegand_data >> 1) & 0xFFFF
        self.assertEqual(extracted_card, card_number)
        
        # Check that parity bits are set
        p0 = (wiegand_data >> 25) & 1
        p1 = (wiegand_data >> 0) & 1
        
        # P0 should be even parity of bits 1-12
        calc_p0 = self.hidprox_calc_parity(wiegand_data, 1, 12, 0)
        self.assertEqual(p0, calc_p0)
        
        # P1 should be odd parity of bits 13-24
        calc_p1 = self.hidprox_calc_parity(wiegand_data, 13, 12, 1)
        self.assertEqual(p1, calc_p1)
    
    def test_invalid_decode(self):
        """Test decoding with invalid parity"""
        # Create a valid encoding
        encoded = self.hidprox_encode(123, 45678)
        
        # Corrupt the data to make parity invalid
        corrupted = bytearray(encoded)
        corrupted[0] ^= 0x01  # Flip a bit
        
        # Should fail to decode due to parity mismatch
        decoded = self.hidprox_decode(bytes(corrupted))
        self.assertIsNone(decoded)


if __name__ == '__main__':
    unittest.main()