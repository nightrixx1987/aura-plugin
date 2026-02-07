#!/bin/bash
# ============================================================================
# Aura EQ - macOS Uninstall Script
# Entfernt Aura vollstaendig vom System
# ============================================================================

echo ""
echo "============================================"
echo "  Aura Uninstaller"
echo "  by Unproved Audio"
echo "============================================"
echo ""

# Warnung
echo "This will remove the following:"
echo "  - /Applications/Aura.app"
echo "  - /Library/Audio/Plug-Ins/VST3/Aura.vst3"
echo "  - ~/Library/Application Support/Unproved Audio/Aura/"
echo ""
read -p "Are you sure? (y/N): " confirm

if [[ "$confirm" != [yY] ]]; then
    echo "Aborted."
    exit 0
fi

echo ""

# Standalone
if [ -d "/Applications/Aura.app" ]; then
    sudo rm -rf "/Applications/Aura.app"
    echo "  [OK] Removed /Applications/Aura.app"
else
    echo "  [--] /Applications/Aura.app not found"
fi

# VST3
if [ -d "/Library/Audio/Plug-Ins/VST3/Aura.vst3" ]; then
    sudo rm -rf "/Library/Audio/Plug-Ins/VST3/Aura.vst3"
    echo "  [OK] Removed VST3 plugin"
else
    echo "  [--] VST3 plugin not found"
fi

# User Settings (optional)
USER_DATA="$HOME/Library/Application Support/Unproved Audio/Aura"
if [ -d "$USER_DATA" ]; then
    read -p "  Remove user presets and settings? (y/N): " remove_data
    if [[ "$remove_data" == [yY] ]]; then
        rm -rf "$USER_DATA"
        echo "  [OK] Removed user data"
        # Leeren Publisher-Ordner aufraumen
        PUBLISHER_DIR="$HOME/Library/Application Support/Unproved Audio"
        if [ -d "$PUBLISHER_DIR" ] && [ -z "$(ls -A "$PUBLISHER_DIR")" ]; then
            rmdir "$PUBLISHER_DIR"
        fi
    else
        echo "  [--] User data preserved"
    fi
fi

# Package Receipt entfernen
pkgutil --pkgs 2>/dev/null | grep "de.unproved-audio" | while read pkg; do
    sudo pkgutil --forget "$pkg" 2>/dev/null
    echo "  [OK] Removed package receipt: $pkg"
done

echo ""
echo "Aura has been uninstalled."
echo "Please restart your DAW to complete the removal."
echo ""
