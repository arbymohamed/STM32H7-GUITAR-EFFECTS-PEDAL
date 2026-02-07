#!/usr/bin/env python3
"""
Convert LVGL image data from C source files to PNG format.
Handles RGB565A8 format (16-bit RGB565 + 8-bit alpha).
"""

import os
import struct
import re
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Error: PIL (Pillow) is required. Install it with: pip install Pillow")
    exit(1)


def extract_image_data(c_file_path):
    """Extract image dimensions, stride, and pixel data from C file."""
    with open(c_file_path, 'r') as f:
        content = f.read()
    
    # Extract width
    width_match = re.search(r'\.w\s*=\s*(\d+)', content)
    if not width_match:
        return None, None, None, None
    width = int(width_match.group(1))
    
    # Extract height
    height_match = re.search(r'\.h\s*=\s*(\d+)', content)
    if not height_match:
        return None, None, None, None
    height = int(height_match.group(1))
    
    # Extract stride
    stride_match = re.search(r'\.stride\s*=\s*(\d+)', content)
    stride = int(stride_match.group(1)) if stride_match else width * 3
    
    # Extract the pixel data array
    data_match = re.search(r'uint8_t\s+\w+_map\[\]\s*=\s*\{(.*?)\};', content, re.DOTALL)
    if not data_match:
        return None, None, None, None
    
    data_str = data_match.group(1)
    # Extract hex values
    hex_values = re.findall(r'0x([0-9a-fA-F]{2})', data_str)
    pixel_data = bytes([int(h, 16) for h in hex_values])
    
    return width, height, stride, pixel_data


def rgb565_to_rgb888(rgb565):
    """Convert RGB565 value to RGB888."""
    r = (rgb565 >> 11) & 0x1F
    g = (rgb565 >> 5) & 0x3F
    b = rgb565 & 0x1F
    
    # Scale to 8-bit
    r = (r << 3) | (r >> 2)
    g = (g << 2) | (g >> 4)
    b = (b << 3) | (b >> 2)
    
    return (r, g, b)


def convert_to_png(c_file_path, output_dir):
    """Convert a single C image file to PNG."""
    width, height, stride, pixel_data = extract_image_data(c_file_path)
    
    if width is None:
        print(f"  ✗ Failed to parse: {os.path.basename(c_file_path)}")
        return False
    
    # Create image
    img = Image.new('RGBA', (width, height), (0, 0, 0, 0))
    pixels = img.load()
    
    # Convert RGB565A8 format to RGBA
    # The stride indicates bytes per row (including padding)
    # Each pixel: 3 bytes (RGB565 in 2 bytes + Alpha in 1 byte)
    for y in range(height):
        row_offset = y * stride
        for x in range(width):
            pixel_offset = row_offset + x * 3
            
            if pixel_offset + 2 < len(pixel_data):
                rgb565_low = pixel_data[pixel_offset]
                rgb565_high = pixel_data[pixel_offset + 1]
                alpha = pixel_data[pixel_offset + 2]
                
                rgb565 = (rgb565_high << 8) | rgb565_low
                r, g, b = rgb565_to_rgb888(rgb565)
                
                pixels[x, y] = (r, g, b, alpha)
            else:
                pixels[x, y] = (0, 0, 0, 0)
    
    # Save as PNG
    base_name = os.path.basename(c_file_path).replace('ui_image_', '').replace('.c', '')
    output_path = os.path.join(output_dir, f"{base_name}.png")
    img.save(output_path, 'PNG')
    print(f"  ✓ {base_name}.png ({width}x{height})")
    return True


def main():
    ui_src_dir = Path(__file__).parent / 'Core' / 'Src' / 'ui'
    output_dir = Path(__file__).parent / 'icons'
    
    if not ui_src_dir.exists():
        print(f"Error: UI directory not found: {ui_src_dir}")
        exit(1)
    
    # Create output directory
    output_dir.mkdir(exist_ok=True)
    
    # Find all image files
    image_files = sorted(ui_src_dir.glob('ui_image_*.c'))
    
    if not image_files:
        print("No image files found!")
        exit(1)
    
    print(f"Converting {len(image_files)} icons to PNG...")
    print(f"Output directory: {output_dir}\n")
    
    success_count = 0
    for c_file in image_files:
        if convert_to_png(str(c_file), str(output_dir)):
            success_count += 1
    
    print(f"\nCompleted: {success_count}/{len(image_files)} icons converted successfully")


if __name__ == '__main__':
    main()
