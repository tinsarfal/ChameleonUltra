#!/usr/bin/env python3
"""
Simple replacement for mergehex using intelhex library.
Usage: python mergehex.py --merge input1.hex input2.hex input3.hex --output output.hex
"""

import argparse
import sys
from intelhex import IntelHex


def main():
    parser = argparse.ArgumentParser(description='Merge Intel HEX files')
    parser.add_argument('--merge', action='store_true', help='Merge hex files')
    parser.add_argument('--output', required=True, help='Output hex file')
    parser.add_argument('input_files', nargs='+', help='Input hex files to merge')
    
    args = parser.parse_args()
    
    if not args.merge:
        print("Error: --merge option is required")
        sys.exit(1)
    
    try:
        # Load the first file
        merged_hex = IntelHex()
        merged_hex.loadhex(args.input_files[0])
        
        # Merge the remaining files
        for hex_file in args.input_files[1:]:
            other_hex = IntelHex()
            other_hex.loadhex(hex_file)
            merged_hex.merge(other_hex, overlap='replace')
        
        # Write the merged output
        merged_hex.write_hex_file(args.output)
        print(f"Successfully merged {len(args.input_files)} files into {args.output}")
        
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()