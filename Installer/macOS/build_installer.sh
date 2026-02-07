#!/bin/bash
# ============================================================================
# Aura EQ - macOS Installer Build Script
# Creates a .pkg installer for Aura by Unproved Audio
# ============================================================================

set -e

# ============================================================================
# Konfiguration
# ============================================================================
APP_NAME="Aura"
APP_VERSION="1.0.0"
PUBLISHER="Unproved Audio"
BUNDLE_ID="de.unproved-audio.aura"
PKG_ID_PREFIX="de.unproved-audio"

# Pfade (relativ zum Projektroot)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build3"
ARTEFACTS_DIR="$BUILD_DIR/Aura_artefacts/Release"
INSTALLER_OUTPUT="$BUILD_DIR/Installer"
STAGING_DIR="$BUILD_DIR/_installer_staging"
RESOURCES_DIR="$SCRIPT_DIR/resources"

# Build-Artefakte
STANDALONE_APP="$ARTEFACTS_DIR/Standalone/Aura.app"
VST3_BUNDLE="$ARTEFACTS_DIR/VST3/Aura.vst3"
CLAP_BUNDLE="$ARTEFACTS_DIR/CLAP/Aura.clap"

# Installer-Ziele
VST3_INSTALL_DIR="/Library/Audio/Plug-Ins/VST3"
CLAP_INSTALL_DIR="/Library/Audio/Plug-Ins/CLAP"
STANDALONE_INSTALL_DIR="/Applications"

# Code Signing (leer lassen fuer unsigniert)
SIGNING_IDENTITY="${SIGNING_IDENTITY:-}"
INSTALLER_SIGNING_IDENTITY="${INSTALLER_SIGNING_IDENTITY:-}"

# ============================================================================
# Hilfsfunktionen
# ============================================================================
log() {
    echo "[$(date '+%H:%M:%S')] $1"
}

error() {
    echo "[ERROR] $1" >&2
    exit 1
}

check_file() {
    if [ ! -e "$1" ]; then
        error "Required file not found: $1"
    fi
}

# ============================================================================
# Voraussetzungen pruefen
# ============================================================================
log "============================================"
log "  Aura macOS Installer Builder v${APP_VERSION}"
log "============================================"
log ""

# Pruefen ob Build-Artefakte existieren
HAS_STANDALONE=false
HAS_VST3=false
HAS_CLAP=false

if [ -d "$STANDALONE_APP" ]; then
    HAS_STANDALONE=true
    log "Found: Standalone App"
else
    log "WARNING: Standalone app not found at $STANDALONE_APP"
fi

if [ -d "$VST3_BUNDLE" ]; then
    HAS_VST3=true
    log "Found: VST3 Plugin"
else
    log "WARNING: VST3 bundle not found at $VST3_BUNDLE"
fi

if [ -f "$CLAP_BUNDLE" ]; then
    HAS_CLAP=true
    log "Found: CLAP Plugin"
else
    log "WARNING: CLAP plugin not found at $CLAP_BUNDLE"
fi

if [ "$HAS_STANDALONE" = false ] && [ "$HAS_VST3" = false ] && [ "$HAS_CLAP" = false ]; then
    error "No build artefacts found. Please build the project first:\n  cmake --build build3 --config Release"
fi

# ============================================================================
# Staging-Verzeichnis vorbereiten
# ============================================================================
log ""
log "Preparing staging directory..."

rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR"
mkdir -p "$INSTALLER_OUTPUT"

# ============================================================================
# Component Packages erstellen
# ============================================================================

# --- Standalone Package ---
if [ "$HAS_STANDALONE" = true ]; then
    log "Building Standalone component package..."
    
    STANDALONE_PAYLOAD="$STAGING_DIR/standalone_payload"
    mkdir -p "$STANDALONE_PAYLOAD/Applications"
    cp -R "$STANDALONE_APP" "$STANDALONE_PAYLOAD/Applications/"
    
    # Code Signing (falls Identity gesetzt)
    if [ -n "$SIGNING_IDENTITY" ]; then
        log "  Signing Standalone app..."
        codesign --force --deep --sign "$SIGNING_IDENTITY" \
            --options runtime \
            "$STANDALONE_PAYLOAD/Applications/Aura.app"
    fi
    
    pkgbuild \
        --root "$STANDALONE_PAYLOAD" \
        --identifier "${PKG_ID_PREFIX}.standalone" \
        --version "$APP_VERSION" \
        --install-location "/" \
        "$STAGING_DIR/AuraStandalone.pkg"
    
    log "  Standalone package created."
fi

# --- VST3 Package ---
if [ "$HAS_VST3" = true ]; then
    log "Building VST3 component package..."
    
    VST3_PAYLOAD="$STAGING_DIR/vst3_payload"
    mkdir -p "$VST3_PAYLOAD${VST3_INSTALL_DIR}"
    cp -R "$VST3_BUNDLE" "$VST3_PAYLOAD${VST3_INSTALL_DIR}/"
    
    # Code Signing (falls Identity gesetzt)
    if [ -n "$SIGNING_IDENTITY" ]; then
        log "  Signing VST3 bundle..."
        codesign --force --deep --sign "$SIGNING_IDENTITY" \
            --options runtime \
            "$VST3_PAYLOAD${VST3_INSTALL_DIR}/Aura.vst3"
    fi
    
    pkgbuild \
        --root "$VST3_PAYLOAD" \
        --identifier "${PKG_ID_PREFIX}.vst3" \
        --version "$APP_VERSION" \
        "$STAGING_DIR/AuraVST3.pkg"
    
    log "  VST3 package created."
fi

# --- CLAP Package ---
if [ "$HAS_CLAP" = true ]; then
    log "Building CLAP component package..."
    
    CLAP_PAYLOAD="$STAGING_DIR/clap_payload"
    mkdir -p "$CLAP_PAYLOAD${CLAP_INSTALL_DIR}"
    cp "$CLAP_BUNDLE" "$CLAP_PAYLOAD${CLAP_INSTALL_DIR}/"
    
    # Code Signing (falls Identity gesetzt)
    if [ -n "$SIGNING_IDENTITY" ]; then
        log "  Signing CLAP plugin..."
        codesign --force --deep --sign "$SIGNING_IDENTITY" \
            --options runtime \
            "$CLAP_PAYLOAD${CLAP_INSTALL_DIR}/Aura.clap"
    fi
    
    pkgbuild \
        --root "$CLAP_PAYLOAD" \
        --identifier "${PKG_ID_PREFIX}.clap" \
        --version "$APP_VERSION" \
        "$STAGING_DIR/AuraCLAP.pkg"
    
    log "  CLAP package created."
fi

# ============================================================================
# Distribution XML erstellen (fuer Komponenten-Auswahl)
# ============================================================================
log "Creating distribution descriptor..."

cat > "$STAGING_DIR/distribution.xml" << 'DISTXML'
<?xml version="1.0" encoding="utf-8" standalone="no"?>
<installer-gui-script minSpecVersion="2">
    <title>Aura - Parametric EQ by Unproved Audio</title>
    <organization>de.unproved-audio</organization>
    
    <!-- Willkommen, Lizenz, Readme -->
    <welcome file="welcome.html" mime-type="text/html"/>
    <license file="license.txt"/>
    <readme file="readme.html" mime-type="text/html"/>
    <conclusion file="conclusion.html" mime-type="text/html"/>
    
    <!-- Hintergrund -->
    <background file="background.png" alignment="bottomleft" scaling="none" mime-type="image/png"/>
    
    <!-- Mindestversion macOS 10.15 -->
    <os-version min="10.15"/>
    
    <!-- Optionen -->
    <options customize="always" require-scripts="false" rootVolumeOnly="true" hostArchitectures="x86_64,arm64"/>
    <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
    
    <!-- Komponenten-Auswahl -->
    <choices-outline>
        <line choice="standalone"/>
        <line choice="vst3"/>
        <line choice="clap"/>
    </choices-outline>
    
DISTXML

# Standalone Choice
if [ "$HAS_STANDALONE" = true ]; then
cat >> "$STAGING_DIR/distribution.xml" << 'DISTXML'
    <choice id="standalone" 
            title="Aura Standalone Application" 
            description="Install Aura as a standalone application in /Applications. Use this to test and configure Aura without a DAW."
            start_selected="true"
            start_enabled="true"
            start_visible="true">
        <pkg-ref id="de.unproved-audio.standalone"/>
    </choice>
    <pkg-ref id="de.unproved-audio.standalone" version="1.0.0" installKBytes="0">AuraStandalone.pkg</pkg-ref>
    
DISTXML
else
cat >> "$STAGING_DIR/distribution.xml" << 'DISTXML'
    <choice id="standalone" 
            title="Aura Standalone Application (not available)" 
            description="Standalone build not found."
            start_selected="false"
            start_enabled="false"
            start_visible="true">
    </choice>
    
DISTXML
fi

# VST3 Choice
if [ "$HAS_VST3" = true ]; then
cat >> "$STAGING_DIR/distribution.xml" << 'DISTXML'
    <choice id="vst3" 
            title="Aura VST3 Plugin" 
            description="Install the Aura VST3 plugin to /Library/Audio/Plug-Ins/VST3. Your DAW will detect Aura after rescanning plugins."
            start_selected="true"
            start_enabled="true"
            start_visible="true">
        <pkg-ref id="de.unproved-audio.vst3"/>
    </choice>
    <pkg-ref id="de.unproved-audio.vst3" version="1.0.0" installKBytes="0">AuraVST3.pkg</pkg-ref>
    
DISTXML
else
cat >> "$STAGING_DIR/distribution.xml" << 'DISTXML'
    <choice id="vst3" 
            title="Aura VST3 Plugin (not available)" 
            description="VST3 build not found."
            start_selected="false"
            start_enabled="false"
            start_visible="true">
    </choice>
    
DISTXML
fi

# CLAP Choice
if [ "$HAS_CLAP" = true ]; then
cat >> "$STAGING_DIR/distribution.xml" << 'DISTXML'
    <choice id="clap" 
            title="Aura CLAP Plugin" 
            description="Install the Aura CLAP plugin to /Library/Audio/Plug-Ins/CLAP. DAWs like Bitwig, Reaper, and others supporting CLAP will detect Aura after rescanning."
            start_selected="true"
            start_enabled="true"
            start_visible="true">
        <pkg-ref id="de.unproved-audio.clap"/>
    </choice>
    <pkg-ref id="de.unproved-audio.clap" version="1.0.0" installKBytes="0">AuraCLAP.pkg</pkg-ref>
    
DISTXML
else
cat >> "$STAGING_DIR/distribution.xml" << 'DISTXML'
    <choice id="clap" 
            title="Aura CLAP Plugin (not available)" 
            description="CLAP build not found."
            start_selected="false"
            start_enabled="false"
            start_visible="true">
    </choice>
    
DISTXML
fi

echo "</installer-gui-script>" >> "$STAGING_DIR/distribution.xml"

# ============================================================================
# Installer-Ressourcen erstellen (HTML-Seiten)
# ============================================================================
log "Creating installer resources..."

INST_RESOURCES="$STAGING_DIR/resources"
mkdir -p "$INST_RESOURCES"

# --- Welcome Page ---
cat > "$INST_RESOURCES/welcome.html" << 'HTML'
<!DOCTYPE html>
<html>
<head>
<style>
body { font-family: -apple-system, BlinkMacSystemFont, 'Helvetica Neue', sans-serif; margin: 20px; color: #333; }
h1 { color: #1a1a2e; font-size: 24px; }
h2 { color: #16213e; font-size: 16px; font-weight: normal; }
.version { color: #888; font-size: 13px; }
.features { margin-top: 15px; }
.features li { margin: 5px 0; }
</style>
</head>
<body>
<h1>Welcome to Aura</h1>
<h2>Professional Parametric EQ by Unproved Audio</h2>
<p class="version">Version 1.0.0</p>
<p>This installer will guide you through the installation of Aura on your Mac.</p>
<div class="features">
<p><strong>Features:</strong></p>
<ul>
<li>12-Band Parametric EQ with multiple filter types</li>
<li>Real-time Spectrum Analyzer</li>
<li>Smart EQ with automatic analysis</li>
<li>EQ Match (Reference Track matching)</li>
<li>Resonance Suppressor (Soothe-style)</li>
<li>Mid/Side processing</li>
<li>Linear Phase EQ mode</li>
<li>VST3, CLAP &amp; Standalone</li>
<li>Oversampling up to 4x</li>
</ul>
</div>
<p><em>Please close all DAWs before continuing.</em></p>
</body>
</html>
HTML

# --- License Page (plain text) ---
if [ -f "$PROJECT_ROOT/LICENSE.txt" ]; then
    cp "$PROJECT_ROOT/LICENSE.txt" "$INST_RESOURCES/license.txt"
else
    echo "AURA END USER LICENSE AGREEMENT - See www.unproved-audio.de for full terms." > "$INST_RESOURCES/license.txt"
fi

# --- Readme Page ---
cat > "$INST_RESOURCES/readme.html" << 'HTML'
<!DOCTYPE html>
<html>
<head>
<style>
body { font-family: -apple-system, BlinkMacSystemFont, 'Helvetica Neue', sans-serif; margin: 20px; color: #333; }
h2 { color: #16213e; }
table { border-collapse: collapse; width: 100%; margin: 10px 0; }
td { padding: 6px 12px; border-bottom: 1px solid #eee; }
td:first-child { font-weight: bold; width: 180px; color: #555; }
.note { background: #f8f9fa; padding: 10px; border-radius: 6px; margin: 10px 0; border-left: 3px solid #4a90d9; }
</style>
</head>
<body>
<h2>Installation Options</h2>
<p>Choose which components to install:</p>
<table>
<tr><td>Standalone App</td><td>Installs to <code>/Applications/Aura.app</code></td></tr>
<tr><td>VST3 Plugin</td><td>Installs to <code>/Library/Audio/Plug-Ins/VST3/Aura.vst3</code></td></tr>
<tr><td>CLAP Plugin</td><td>Installs to <code>/Library/Audio/Plug-Ins/CLAP/Aura.clap</code></td></tr>
</table>
<div class="note">
<strong>Note:</strong> After installing the VST3 plugin, you may need to rescan your plugins in your DAW.
</div>
<h2>System Requirements</h2>
<table>
<tr><td>macOS</td><td>10.15 (Catalina) or later</td></tr>
<tr><td>Architecture</td><td>Intel (x86_64) and Apple Silicon (arm64)</td></tr>
<tr><td>DAW</td><td>Any VST3-compatible DAW</td></tr>
</table>
</body>
</html>
HTML

# --- Conclusion Page ---
cat > "$INST_RESOURCES/conclusion.html" << 'HTML'
<!DOCTYPE html>
<html>
<head>
<style>
body { font-family: -apple-system, BlinkMacSystemFont, 'Helvetica Neue', sans-serif; margin: 20px; color: #333; }
h1 { color: #1a1a2e; }
.success { color: #27ae60; font-size: 18px; font-weight: bold; }
.info { background: #f0f7ff; padding: 12px; border-radius: 6px; margin: 15px 0; }
.steps { margin: 10px 0; }
.steps li { margin: 8px 0; }
a { color: #4a90d9; }
</style>
</head>
<body>
<h1>Installation Complete!</h1>
<p class="success">✅ Aura has been installed successfully.</p>
<div class="info">
<p><strong>Next Steps:</strong></p>
<ul class="steps">
<li>Open your DAW and rescan VST3 plugins</li>
<li>Find "Aura" by Unproved Audio in your plugin list</li>
<li>The trial version is fully functional for 30 days</li>
</ul>
</div>
<p>For support, visit <a href="https://www.unproved-audio.de">www.unproved-audio.de</a></p>
<p><em>Thank you for choosing Aura!</em></p>
</body>
</html>
HTML

# Background-Bild (optional, wird uebersprungen wenn nicht vorhanden)
if [ -f "$PROJECT_ROOT/Resources/icon_256.png" ]; then
    cp "$PROJECT_ROOT/Resources/icon_256.png" "$INST_RESOURCES/background.png"
else
    log "  No background image found, skipping..."
    # Entferne background-Zeile aus distribution.xml
    sed -i '' '/<background/d' "$STAGING_DIR/distribution.xml"
fi

# ============================================================================
# Finales Installer-Paket bauen
# ============================================================================
log ""
log "Building final installer package..."

FINAL_PKG="$INSTALLER_OUTPUT/Aura_Setup_v${APP_VERSION}_macOS.pkg"

PRODUCTBUILD_ARGS=(
    --distribution "$STAGING_DIR/distribution.xml"
    --resources "$INST_RESOURCES"
    --package-path "$STAGING_DIR"
    --version "$APP_VERSION"
)

# Installer signieren (falls Identity gesetzt)
if [ -n "$INSTALLER_SIGNING_IDENTITY" ]; then
    log "  Signing installer package..."
    PRODUCTBUILD_ARGS+=(--sign "$INSTALLER_SIGNING_IDENTITY")
fi

productbuild "${PRODUCTBUILD_ARGS[@]}" "$FINAL_PKG"

log ""
log "============================================"
log "  Installer created successfully!"
log "  Output: $FINAL_PKG"
log "  Size:   $(du -h "$FINAL_PKG" | cut -f1)"
log "============================================"

# ============================================================================
# Aufraumen
# ============================================================================
log "Cleaning up staging directory..."
rm -rf "$STAGING_DIR"

# ============================================================================
# Optional: DMG erstellen
# ============================================================================
if [ "${CREATE_DMG:-false}" = true ]; then
    log ""
    log "Creating DMG image..."
    
    DMG_STAGING="$BUILD_DIR/_dmg_staging"
    DMG_OUTPUT="$INSTALLER_OUTPUT/Aura_v${APP_VERSION}_macOS.dmg"
    
    rm -rf "$DMG_STAGING"
    mkdir -p "$DMG_STAGING"
    cp "$FINAL_PKG" "$DMG_STAGING/"
    
    # README in DMG
    cat > "$DMG_STAGING/README.txt" << EOF
Aura v${APP_VERSION} - Parametric EQ
by Unproved Audio

Double-click the .pkg file to start the installer.
Visit www.unproved-audio.de for support.
EOF
    
    hdiutil create \
        -volname "Aura v${APP_VERSION}" \
        -srcfolder "$DMG_STAGING" \
        -ov \
        -format UDZO \
        "$DMG_OUTPUT"
    
    rm -rf "$DMG_STAGING"
    
    log "DMG created: $DMG_OUTPUT"
fi

log ""
log "Done!"
