#!/usr/bin/env python3
"""
HID Prox CLI Demo

This script demonstrates how to use the HID Prox commands in the Chameleon Ultra CLI.

Commands added:
- lf hidprox read       : Scan for HID Prox tags and display facility code and card number
- lf hidprox econfig    : Set/get emulation parameters for HID Prox

Examples:
1. Scan for HID Prox tags:
   lf hidprox read

2. Set HID Prox emulation:
   lf hidprox econfig -f 123 -c 45678

3. Get current HID Prox emulation settings:
   lf hidprox econfig

4. Set only facility code (keeps current card number):
   lf hidprox econfig -f 200

5. Set only card number (keeps current facility code):
   lf hidprox econfig -c 12345

6. Use with specific slot:
   lf hidprox econfig -s 2 -f 100 -c 9999

Supported ranges:
- Facility Code: 0-255 (8 bits)
- Card Number: 0-65535 (16 bits)

Note: This implementation uses 26-bit Wiegand format which is common for HID Prox cards.
The format is: [P0][8-bit facility][16-bit card number][P1]
where P0 is even parity for bits 1-12 and P1 is odd parity for bits 13-24.
"""

import sys
import os

# Add the script directory to Python path to import modules
script_dir = os.path.dirname(os.path.abspath(__file__))
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)

def main():
    print(__doc__)
    
    print("\nTo test the HID Prox functionality:")
    print("1. Run the Chameleon CLI: python3 chameleon_cli_main.py")
    print("2. Try the commands listed above")
    print("\nAvailable HID Prox commands:")
    print("  lf hidprox read              - Scan HID Prox tag")
    print("  lf hidprox econfig           - Get emulation settings")
    print("  lf hidprox econfig -f 123 -c 45678  - Set emulation")
    print("  lf hidprox econfig --help    - Show detailed help")


if __name__ == "__main__":
    main()