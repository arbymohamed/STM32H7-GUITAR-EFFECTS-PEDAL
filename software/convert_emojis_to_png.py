#!/usr/bin/env python3
"""
Convert emoji icons from HTML mockup to PNG files.
"""

import os
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Error: PIL (Pillow) is required. Install it with: pip install Pillow")
    exit(1)


def emoji_to_png(emoji, filename, size=64, output_dir="icons"):
    """Convert an emoji character to a PNG file."""
    # Create a new image with transparent background
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Try to use a system font that supports emoji
    font_size = int(size * 0.8)
    try:
        # Try common emoji font paths on Linux
        font_paths = [
            "/usr/share/fonts/opentype/noto/NotoColorEmoji.ttf",
            "/usr/share/fonts/opentype/noto/NotoEmoji-Regular.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        ]
        
        font = None
        for font_path in font_paths:
            if os.path.exists(font_path):
                try:
                    font = ImageFont.truetype(font_path, font_size)
                    break
                except:
                    continue
        
        if font is None:
            # Fall back to default font
            font = ImageFont.load_default()
    except:
        font = ImageFont.load_default()
    
    # Get emoji bounding box for centering
    bbox = draw.textbbox((0, 0), emoji, font=font)
    emoji_width = bbox[2] - bbox[0]
    emoji_height = bbox[3] - bbox[1]
    
    # Calculate centered position
    x = (size - emoji_width) // 2
    y = (size - emoji_height) // 2
    
    # Draw the emoji
    draw.text((x, y), emoji, font=font, fill=(255, 255, 255, 255))
    
    # Create output directory if it doesn't exist
    Path(output_dir).mkdir(exist_ok=True)
    
    # Save the image
    output_path = os.path.join(output_dir, filename)
    img.save(output_path, 'PNG')
    print(f"  ✓ {filename}")


def main():
    # Emoji icons from the HTML mockup
    emojis = {
        "presets.png": ("🎹", 64),
        "edit.png": ("🔧", 64),
        "amp.png": ("🎚️", 64),
        "cabinet.png": ("🔊", 64),
        "speaker_small.png": ("🔊", 32),
    }
    
    output_dir = "icons"
    
    print(f"Converting emoji icons to PNG...")
    print(f"Output directory: {output_dir}\n")
    
    for filename, (emoji, size) in emojis.items():
        emoji_to_png(emoji, filename, size=size, output_dir=output_dir)
    
    print(f"\nCompleted: {len(emojis)} emoji icons converted successfully")


if __name__ == '__main__':
    main()
