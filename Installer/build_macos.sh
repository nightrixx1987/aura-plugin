#!/bin/bash
# ============================================================================
# Aura EQ - macOS Build & Installer Script
# Erstellt universelles Binary (Intel + Apple Silicon) und .pkg Installer
# ============================================================================
# Verwendung: ./build_macos.sh
# Voraussetzungen: Xcode, CMake (brew install cmake)
# ============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build_macos"
VERSION="1.0.0"
APP_NAME="Aura"
PUBLISHER="Unproved Audio"
IDENTIFIER="de.unproved-audio.aura"

echo "============================================"
echo "  $APP_NAME v$VERSION - macOS Build"
echo "  $PUBLISHER"
echo "============================================"
echo ""

# ============================================================================
# Schritt 1: CMake konfigurieren
# ============================================================================
echo "[1/5] CMake konfigurieren..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$PROJECT_DIR" \
    -G "Xcode" \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.15"

echo "  -> OK"

# ============================================================================
# Schritt 2: Release Build
# ============================================================================
echo "[2/5] Release Build..."
cmake --build . --config Release -- -quiet

echo "  -> OK"

# ============================================================================
# Schritt 3: Build-Ergebnisse prüfen
# ============================================================================
echo "[3/5] Build-Ergebnisse pruefen..."

STANDALONE="$BUILD_DIR/Aura_artefacts/Release/Standalone/Aura.app"
VST3="$BUILD_DIR/Aura_artefacts/Release/VST3/Aura.vst3"

if [ ! -d "$STANDALONE" ]; then
    echo "  FEHLER: Standalone App nicht gefunden: $STANDALONE"
    exit 1
fi

if [ ! -d "$VST3" ]; then
    echo "  FEHLER: VST3 Plugin nicht gefunden: $VST3"
    exit 1
fi

echo "  Standalone: $STANDALONE"
echo "  VST3:       $VST3"

# Architektur prüfen
echo "  Architectures:"
lipo -archs "$STANDALONE/Contents/MacOS/Aura" 2>/dev/null || echo "    (konnte nicht geprüft werden)"

echo "  -> OK"

# ============================================================================
# Schritt 4: .pkg Installer erstellen
# ============================================================================
echo "[4/5] macOS Installer (.pkg) erstellen..."

PKG_DIR="$BUILD_DIR/pkg_staging"
INSTALLER_DIR="$BUILD_DIR/Installer"
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR"
mkdir -p "$INSTALLER_DIR"

# --- Standalone App -> /Applications ---
STANDALONE_PKG_ROOT="$PKG_DIR/standalone_root"
mkdir -p "$STANDALONE_PKG_ROOT/Applications/$PUBLISHER"
cp -R "$STANDALONE" "$STANDALONE_PKG_ROOT/Applications/$PUBLISHER/"

# --- VST3 Plugin -> ~/Library/Audio/Plug-Ins/VST3 ---
VST3_PKG_ROOT="$PKG_DIR/vst3_root"
mkdir -p "$VST3_PKG_ROOT/Library/Audio/Plug-Ins/VST3"
cp -R "$VST3" "$VST3_PKG_ROOT/Library/Audio/Plug-Ins/VST3/"

# Component Packages erstellen
echo "  Erstelle Standalone Component..."
pkgbuild \
    --root "$STANDALONE_PKG_ROOT" \
    --identifier "$IDENTIFIER.standalone" \
    --version "$VERSION" \
    --install-location "/" \
    "$PKG_DIR/AuraStandalone.pkg"

echo "  Erstelle VST3 Component..."
pkgbuild \
    --root "$VST3_PKG_ROOT" \
    --identifier "$IDENTIFIER.vst3" \
    --version "$VERSION" \
    --install-location "/" \
    "$PKG_DIR/AuraVST3.pkg"

# --- Distribution XML ---
cat > "$PKG_DIR/distribution.xml" << 'DISTXML'
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Aura EQ by Unproved Audio</title>
    <organization>de.unproved-audio</organization>
    <welcome file="welcome.html"/>
    <license file="license.txt"/>
    <options customize="allow" require-scripts="false" hostArchitectures="x86_64,arm64"/>
    
    <choices-outline>
        <line choice="standalone"/>
        <line choice="vst3"/>
    </choices-outline>
    
    <choice id="standalone" 
            title="Aura Standalone Application"
            description="Installs the standalone app to /Applications/Unproved Audio/"
            selected="true">
        <pkg-ref id="de.unproved-audio.aura.standalone"/>
    </choice>
    
    <choice id="vst3"
            title="Aura VST3 Plugin"
            description="Installs the VST3 plugin to /Library/Audio/Plug-Ins/VST3/"
            selected="true">
        <pkg-ref id="de.unproved-audio.aura.vst3"/>
    </choice>
    
    <pkg-ref id="de.unproved-audio.aura.standalone" version="1.0.0">AuraStandalone.pkg</pkg-ref>
    <pkg-ref id="de.unproved-audio.aura.vst3" version="1.0.0">AuraVST3.pkg</pkg-ref>
</installer-gui-script>
DISTXML

# --- Welcome HTML ---
cat > "$PKG_DIR/welcome.html" << 'WELCOME'
<html>
<body style="font-family: -apple-system, Helvetica, Arial, sans-serif; font-size: 13px;">
<h2>Aura EQ v1.0.0</h2>
<p>Willkommen zum Aura EQ Installer von <strong>Unproved Audio</strong>.</p>
<p>Dieses Paket installiert:</p>
<ul>
    <li><strong>Aura Standalone</strong> — nach /Applications/Unproved Audio/</li>
    <li><strong>Aura VST3</strong> — nach /Library/Audio/Plug-Ins/VST3/</li>
</ul>
<p>Unterstützt Intel und Apple Silicon Macs (Universal Binary).</p>
</body>
</html>
WELCOME

# --- License ---
cp "$PROJECT_DIR/LICENSE.txt" "$PKG_DIR/license.txt"

# Finalen Product Installer erstellen
echo "  Erstelle finalen Installer..."
productbuild \
    --distribution "$PKG_DIR/distribution.xml" \
    --package-path "$PKG_DIR" \
    --resources "$PKG_DIR" \
    "$INSTALLER_DIR/Aura_Setup_v${VERSION}_macOS.pkg"

echo "  -> OK"

# ============================================================================
# Schritt 5: Ergebnis
# ============================================================================
echo ""
echo "============================================"
echo "  BUILD ERFOLGREICH!"
echo "============================================"
echo ""

INSTALLER_FILE="$INSTALLER_DIR/Aura_Setup_v${VERSION}_macOS.pkg"
if [ -f "$INSTALLER_FILE" ]; then
    SIZE=$(du -h "$INSTALLER_FILE" | cut -f1)
    echo "  Installer: $INSTALLER_FILE"
    echo "  Groesse:   $SIZE"
else
    echo "  WARNUNG: Installer-Datei nicht gefunden!"
fi

echo ""
echo "  Optional: Zum Signieren:"
echo "  productsign --sign \"Developer ID Installer: Your Name\" \\"
echo "    \"$INSTALLER_FILE\" \\"
echo "    \"${INSTALLER_FILE%.pkg}_signed.pkg\""
echo ""
echo "  Zum Notarisieren:"
echo "  xcrun notarytool submit \"$INSTALLER_FILE\" \\"
echo "    --apple-id YOUR_APPLE_ID \\"
echo "    --password YOUR_APP_PASSWORD \\"
echo "    --team-id YOUR_TEAM_ID"
echo ""
