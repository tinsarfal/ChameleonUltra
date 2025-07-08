#!/usr/bin/env python3

import sys
import unittest
import struct
sys.path.append('..')

from chameleon_cmd import ChameleonCMD       # noqa: E402
from chameleon_enum import Command, Status   # noqa: E402
import chameleon_com                        # noqa: E402


class MockChameleonCom:
    """Mock communication class for testing HID Prox commands without hardware"""
    
    def __init__(self):
        self.test_facility_code = 123
        self.test_card_number = 45678
    
    def send_cmd_sync(self, command, data=None):
        """Mock send command implementation"""
        response = chameleon_com.Response(cmd=command, status=Status.SUCCESS, data=b'')
        
        if command == Command.HIDPROX_SCAN:
            # Mock successful scan response
            response.status = Status.LF_TAG_OK
            response.data = struct.pack('!BH', self.test_facility_code, self.test_card_number)
            
        elif command == Command.HIDPROX_SET_EMU_ID:
            # Mock successful set emulation ID
            if data and len(data) == 4:
                facility_code, card_number, _ = struct.unpack('!BHB', data)
                self.test_facility_code = facility_code
                self.test_card_number = card_number
                response.status = Status.SUCCESS
            else:
                response.status = Status.PAR_ERR
                
        elif command == Command.HIDPROX_GET_EMU_ID:
            # Mock get emulation ID response
            response.status = Status.SUCCESS
            response.data = struct.pack('!BHB', self.test_facility_code, self.test_card_number, 0)
        
        return response


class TestHIDProxCMD(unittest.TestCase):
    """Test HID Prox command functionality"""
    
    def setUp(self):
        self.mock_com = MockChameleonCom()
        self.cmd = ChameleonCMD(self.mock_com)
    
    def test_hidprox_scan(self):
        """Test HID Prox scan functionality"""
        result = self.cmd.hidprox_scan()
        
        # The @expect_response decorator returns the parsed data directly
        self.assertIsNotNone(result)
        self.assertEqual(result['facility_code'], 123)
        self.assertEqual(result['card_number'], 45678)
    
    def test_hidprox_set_emu_id_valid(self):
        """Test setting valid HID Prox emulation ID"""
        facility_code = 200
        card_number = 12345
        
        # set_emu_id returns None on success (no parsed data)
        result = self.cmd.hidprox_set_emu_id(facility_code, card_number)
        # Should not raise exception if successful
        
        # Verify the values were set
        get_result = self.cmd.hidprox_get_emu_id()
        self.assertEqual(get_result['facility_code'], facility_code)
        self.assertEqual(get_result['card_number'], card_number)
    
    def test_hidprox_set_emu_id_invalid_facility(self):
        """Test setting invalid facility code (out of range)"""
        with self.assertRaises(ValueError):
            self.cmd.hidprox_set_emu_id(256, 12345)  # facility > 255
        
        with self.assertRaises(ValueError):
            self.cmd.hidprox_set_emu_id(-1, 12345)   # facility < 0
    
    def test_hidprox_set_emu_id_invalid_card(self):
        """Test setting invalid card number (out of range)"""
        with self.assertRaises(ValueError):
            self.cmd.hidprox_set_emu_id(123, 65536)  # card > 65535
        
        with self.assertRaises(ValueError):
            self.cmd.hidprox_set_emu_id(123, -1)     # card < 0
    
    def test_hidprox_get_emu_id(self):
        """Test getting HID Prox emulation ID"""
        result = self.cmd.hidprox_get_emu_id()
        
        # The @expect_response decorator returns the parsed data directly
        self.assertIsNotNone(result)
        self.assertIn('facility_code', result)
        self.assertIn('card_number', result)
        self.assertEqual(result['facility_code'], 123)
        self.assertEqual(result['card_number'], 45678)
    
    def test_hidprox_boundary_values(self):
        """Test boundary values for facility code and card number"""
        # Test minimum values
        self.cmd.hidprox_set_emu_id(0, 0)  # Should not raise exception
        
        get_result = self.cmd.hidprox_get_emu_id()
        self.assertEqual(get_result['facility_code'], 0)
        self.assertEqual(get_result['card_number'], 0)
        
        # Test maximum values
        self.cmd.hidprox_set_emu_id(255, 65535)  # Should not raise exception
        
        get_result = self.cmd.hidprox_get_emu_id()
        self.assertEqual(get_result['facility_code'], 255)
        self.assertEqual(get_result['card_number'], 65535)


if __name__ == '__main__':
    unittest.main()