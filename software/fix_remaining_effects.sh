#!/bin/bash
# Script to apply volatile qualifiers and memory barriers to all effect files
# Run from project root: ./fix_remaining_effects.sh

echo "========================================="
echo "Applying thread-safety fixes to effects"
echo "========================================="

# List of effect files that need fixing
effects=(
    "chorus"
    "tremolo"
    "flanger"
    "phaser"
    "distortion"
    "overdrive"
    "autowah"
    "hall_reverb"
    "plate_reverb"
    "spring_reverb"
    "shimmer_reverb"
)

for effect in "${effects[@]}"; do
    file="Core/Src/effects/${effect}.c"
    
    if [ ! -f "$file" ]; then
        echo "⚠️  Skipping $file (not found)"
        continue
    fi
    
    echo "📝 Processing $file..."
    
    # Step 1: Add volatile to static float parameters
    # Match lines like: "static float param_name = value;"
    # Replace with: "static volatile float param_name = value;"
    sed -i 's/^static float \([a-z_]*\) = /static volatile float \1 = /g' "$file"
    
    echo "   ✅ Added volatile qualifiers"
    
    # Step 2: Add memory barriers to setter functions
    # This is trickier - we'll add __DMB() before the closing brace of functions
    # that contain parameter assignments
    
    # Note: This is a simplified approach. Manual review is still recommended!
    echo "   ⚠️  Manual review needed for __DMB() placement"
done

echo ""
echo "========================================="
echo "✅ Volatile qualifiers applied!"
echo "========================================="
echo ""
echo "⚠️  IMPORTANT: You must manually add __DMB() to setter functions!"
echo ""
echo "Template for each setter function:"
echo ""
echo "void Effect_SetParameter(float value) {"
echo "    // ... clamping/validation ..."
echo "    param_variable = value;"
echo "    __DMB();  // <-- ADD THIS LINE"
echo "}"
echo ""
echo "Files to review:"
for effect in "${effects[@]}"; do
    echo "  - Core/Src/effects/${effect}.c"
done
echo ""
