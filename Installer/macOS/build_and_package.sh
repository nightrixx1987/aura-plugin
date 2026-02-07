#!/bin/bash
# ============================================================================
# Aura EQ - macOS Build & Package Script
# Baut das Projekt und erstellt den Installer (.pkg)
# ============================================================================

set -e

# ============================================================================
# Konfiguration
# ============================================================================
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build3"
CONFIG="${CONFIG:-Release}"
CREATE_DMG="${CREATE_DMG:-false}"

# Farb-Ausgabe
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log()  { echo -e "${CYAN}[BUILD]${NC} $1"; }
ok()   { echo -e "  ${GREEN}[OK]${NC} $1"; }
warn() { echo -e "  ${YELLOW}[!]${NC} $1"; }
err()  { echo -e "  ${RED}[ERROR]${NC} $1" >&2; exit 1; }

# ============================================================================
# Argumente parsen
# ============================================================================
BUILD_ONLY=false
INSTALLER_ONLY=false
CLEAN=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-only)   BUILD_ONLY=true; shift ;;
        --installer-only) INSTALLER_ONLY=true; shift ;;
        --clean)        CLEAN=true; shift ;;
        --debug)        CONFIG="Debug"; shift ;;
        --dmg)          CREATE_DMG=true; shift ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --build-only     Only build, don't create installer"
            echo "  --installer-only Only create installer from existing build"
            echo "  --clean          Clean build directory before building"
            echo "  --debug          Build in Debug configuration"
            echo "  --dmg            Also create a DMG disk image"
            echo "  -h, --help       Show this help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

echo ""
echo "============================================"
echo "  Aura Build & Package Script (macOS)"
echo "  Configuration: $CONFIG"
echo "============================================"
echo ""

# ============================================================================
# 1. Voraussetzungen pruefen
# ============================================================================
log "Checking prerequisites..."

# CMake
if ! command -v cmake &> /dev/null; then
    err "CMake not found. Install with: brew install cmake"
fi
ok "CMake: $(cmake --version | head -1)"

# Xcode Command Line Tools
if ! xcode-select -p &> /dev/null; then
    err "Xcode Command Line Tools not found. Install with: xcode-select --install"
fi
ok "Xcode Tools: $(xcode-select -p)"

# pkgbuild / productbuild
if ! command -v pkgbuild &> /dev/null; then
    err "pkgbuild not found (should be part of Xcode Command Line Tools)"
fi
ok "pkgbuild: available"

# ============================================================================
# 2. Clean (optional)
# ============================================================================
if [ "$CLEAN" = true ]; then
    log "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    ok "Build directory cleaned."
fi

# ============================================================================
# 3. CMake Configure & Build
# ============================================================================
if [ "$INSTALLER_ONLY" = false ]; then
    log "Configuring CMake..."
    
    mkdir -p "$BUILD_DIR"
    
    # Universelle Binary (Intel + Apple Silicon)
    cmake -B "$BUILD_DIR" \
        -G "Xcode" \
        -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="10.15" \
        "$PROJECT_ROOT"
    
    ok "CMake configured."
    
    log "Building Aura ($CONFIG)..."
    
    cmake --build "$BUILD_DIR" --config "$CONFIG" -- -quiet
    
    ok "Build completed."
    
    # Artefakte pruefen
    STANDALONE="$BUILD_DIR/Aura_artefacts/$CONFIG/Standalone/Aura.app"
    VST3="$BUILD_DIR/Aura_artefacts/$CONFIG/VST3/Aura.vst3"
    CLAP="$BUILD_DIR/Aura_artefacts/$CONFIG/CLAP/Aura.clap"
    
    if [ -d "$STANDALONE" ]; then
        SIZE=$(du -sh "$STANDALONE" | cut -f1)
        ok "Standalone: $SIZE"
    else
        warn "Standalone app not found"
    fi
    
    if [ -d "$VST3" ]; then
        SIZE=$(du -sh "$VST3" | cut -f1)
        ok "VST3 Bundle: $SIZE"
    else
        warn "VST3 bundle not found"
    fi
    
    if [ -f "$CLAP" ]; then
        SIZE=$(du -sh "$CLAP" | cut -f1)
        ok "CLAP Plugin: $SIZE"
    else
        warn "CLAP plugin not found"
    fi
fi

if [ "$BUILD_ONLY" = true ]; then
    echo ""
    echo "============================================"
    echo "  Build complete!"
    echo "============================================"
    exit 0
fi

# ============================================================================
# 4. Installer erstellen
# ============================================================================
log "Building macOS Installer..."

INSTALLER_SCRIPT="$SCRIPT_DIR/build_installer.sh"

if [ ! -f "$INSTALLER_SCRIPT" ]; then
    err "Installer script not found: $INSTALLER_SCRIPT"
fi

# Installer-Script ausfuehren
export CREATE_DMG
bash "$INSTALLER_SCRIPT"

# ============================================================================
# Zusammenfassung
# ============================================================================
echo ""
echo "============================================"
echo "  Build & Package Complete!"
echo "============================================"
echo ""

INSTALLER_OUTPUT="$BUILD_DIR/Installer"
if [ -d "$INSTALLER_OUTPUT" ]; then
    echo "  Output files:"
    for f in "$INSTALLER_OUTPUT"/Aura_*; do
        if [ -f "$f" ]; then
            SIZE=$(du -h "$f" | cut -f1)
            echo "    $(basename "$f") ($SIZE)"
        fi
    done
fi
echo ""
