# ============================================================================
# Aura EQ - Windows Build & Package Script
# Baut das Projekt und erstellt den Installer
# ============================================================================

param(
    [switch]$BuildOnly,
    [switch]$InstallerOnly,
    [switch]$Clean,
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

# ============================================================================
# Konfiguration
# ============================================================================
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$BuildDir = Join-Path $ProjectRoot "build3"
$InstallerDir = Join-Path $ProjectRoot "Installer"
$InstallerOutput = Join-Path $BuildDir "Installer"

# Farb-Ausgabe
function Write-Step($msg) { Write-Host "`n[BUILD] $msg" -ForegroundColor Cyan }
function Write-Ok($msg) { Write-Host "  [OK] $msg" -ForegroundColor Green }
function Write-Warn($msg) { Write-Host "  [!] $msg" -ForegroundColor Yellow }
function Write-Err($msg) { Write-Host "  [ERROR] $msg" -ForegroundColor Red }

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Aura Build & Package Script (Windows)" -ForegroundColor Cyan
Write-Host "  Configuration: $Config" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

# ============================================================================
# 1. Voraussetzungen pruefen
# ============================================================================
Write-Step "Checking prerequisites..."

# CMake
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    Write-Err "CMake not found. Please install CMake and add to PATH."
    exit 1
}
Write-Ok "CMake: $($cmake.Source)"

# Visual Studio / MSBuild
$msbuild = Get-Command msbuild -ErrorAction SilentlyContinue
if (-not $msbuild) {
    # Versuche VS Developer Command Prompt zu finden
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsPath = & $vsWhere -latest -requires Microsoft.Component.MSBuild -property installationPath 2>$null
        if ($vsPath) {
            $msbuildPath = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"
            if (Test-Path $msbuildPath) {
                Write-Ok "MSBuild: $msbuildPath"
            }
        }
    }
    if (-not (Test-Path $msbuildPath -ErrorAction SilentlyContinue)) {
        Write-Warn "MSBuild not found in PATH. CMake will use its own generator."
    }
}
else {
    Write-Ok "MSBuild: $($msbuild.Source)"
}

# Inno Setup (fuer Installer)
$innoSetup = $null
$innoPaths = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe"
)
foreach ($p in $innoPaths) {
    if (Test-Path $p) {
        $innoSetup = $p
        break
    }
}
if ($innoSetup) {
    Write-Ok "Inno Setup: $innoSetup"
}
else {
    Write-Warn "Inno Setup 6 not found. Installer will not be built."
    Write-Warn "Download from: https://jrsoftware.org/isinfo.php"
}

# ============================================================================
# 2. Clean (optional)
# ============================================================================
if ($Clean) {
    Write-Step "Cleaning build directory..."
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
        Write-Ok "Build directory cleaned."
    }
}

# ============================================================================
# 3. CMake Configure
# ============================================================================
if (-not $InstallerOnly) {
    Write-Step "Configuring CMake..."

    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
    }

    Push-Location $ProjectRoot
    try {
        cmake -B build3 -G "Visual Studio 18 2026" -A x64
        if ($LASTEXITCODE -ne 0) {
            Write-Err "CMake configure failed!"
            exit 1
        }
        Write-Ok "CMake configured successfully."
    }
    finally {
        Pop-Location
    }

    # ============================================================================
    # 4. Build
    # ============================================================================
    Write-Step "Building Aura ($Config)..."

    cmake --build $BuildDir --config $Config --parallel
    if ($LASTEXITCODE -ne 0) {
        Write-Err "Build failed!"
        exit 1
    }
    Write-Ok "Build completed successfully."

    # Pruefen ob Artefakte existieren
    $standaloneExe = Join-Path $BuildDir "Aura_artefacts\$Config\Standalone\Aura.exe"
    $vst3Dir = Join-Path $BuildDir "Aura_artefacts\$Config\VST3\Aura.vst3"
    $clapFile = Join-Path $BuildDir "Aura_artefacts\$Config\CLAP\Aura.clap"

    if (Test-Path $standaloneExe) {
        $size = (Get-Item $standaloneExe).Length / 1MB
        Write-Ok ("Standalone: {0:N1} MB" -f $size)
    }
    else {
        Write-Warn "Standalone exe not found at $standaloneExe"
    }

    if (Test-Path $vst3Dir) {
        Write-Ok "VST3 Bundle: $vst3Dir"
    }
    else {
        Write-Warn "VST3 bundle not found at $vst3Dir"
    }

    if (Test-Path $clapFile) {
        $size = (Get-Item $clapFile).Length / 1MB
        Write-Ok ("CLAP Plugin: {0:N1} MB" -f $size)
    }
    else {
        Write-Warn "CLAP plugin not found at $clapFile"
    }
}

if ($BuildOnly) {
    Write-Host "`n============================================" -ForegroundColor Green
    Write-Host "  Build complete!" -ForegroundColor Green
    Write-Host "============================================" -ForegroundColor Green
    exit 0
}

# ============================================================================
# 5. Installer erstellen
# ============================================================================
if ($innoSetup) {
    Write-Step "Building Windows Installer..."

    if (-not (Test-Path $InstallerOutput)) {
        New-Item -ItemType Directory -Path $InstallerOutput -Force | Out-Null
    }

    $issFile = Join-Path $InstallerDir "AuraSetup.iss"
    if (-not (Test-Path $issFile)) {
        Write-Err "Inno Setup script not found: $issFile"
        exit 1
    }

    Push-Location $InstallerDir
    try {
        & $innoSetup $issFile
        if ($LASTEXITCODE -ne 0) {
            Write-Err "Installer build failed!"
            exit 1
        }
    }
    finally {
        Pop-Location
    }

    # Installer-Datei finden
    $installerFile = Get-ChildItem -Path $InstallerOutput -Filter "Aura_Setup_*.exe" |
                     Sort-Object LastWriteTime -Descending |
                     Select-Object -First 1

    if ($installerFile) {
        $size = $installerFile.Length / 1MB
        Write-Ok ("Installer created: {0} ({1:N1} MB)" -f $installerFile.Name, $size)
        Write-Ok "Location: $($installerFile.FullName)"
    }
    else {
        Write-Warn "Installer file not found in output directory."
    }
}
else {
    Write-Warn "Skipping installer (Inno Setup not found)."
}

# ============================================================================
# Zusammenfassung
# ============================================================================
Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "  Build & Package Complete!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host ""

$artefacts = Join-Path $BuildDir "Aura_artefacts\$Config"
if (Test-Path $artefacts) {
    Write-Host "  Build Artefacts:" -ForegroundColor White
    
    $sa = Join-Path $artefacts "Standalone\Aura.exe"
    if (Test-Path $sa) {
        Write-Host "    Standalone: $sa" -ForegroundColor Gray
    }
    
    $v3 = Join-Path $artefacts "VST3\Aura.vst3"
    if (Test-Path $v3) {
        Write-Host "    VST3:       $v3" -ForegroundColor Gray
    }
    
    $cl = Join-Path $artefacts "CLAP\Aura.clap"
    if (Test-Path $cl) {
        Write-Host "    CLAP:       $cl" -ForegroundColor Gray
    }
}

if (Test-Path $InstallerOutput) {
    $installers = Get-ChildItem -Path $InstallerOutput -Filter "*.exe" | Sort-Object LastWriteTime -Descending
    if ($installers) {
        Write-Host ""
        Write-Host "  Installer:" -ForegroundColor White
        foreach ($inst in $installers) {
            Write-Host "    $($inst.FullName)" -ForegroundColor Gray
        }
    }
}

Write-Host ""
