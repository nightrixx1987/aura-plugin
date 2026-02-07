; ============================================================================
; Aura EQ - Professional Installer Script
; Inno Setup 6.x Script for Aura by Unproved Audio
; ============================================================================

#define MyAppName "Aura"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Unproved Audio"
#define MyAppURL "https://www.unproved-audio.de"
#define MyAppExeName "Aura.exe"
#define MyAppCopyright "Copyright (c) 2024-2026 Unproved Audio"

[Setup]
; Basis-Einstellungen
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
AppCopyright={#MyAppCopyright}
VersionInfoVersion={#MyAppVersion}.0
VersionInfoCompany={#MyAppPublisher}
VersionInfoProductName={#MyAppName}

; Standard-Installationspfad
DefaultDirName={autopf}\{#MyAppPublisher}\{#MyAppName}
DefaultGroupName={#MyAppPublisher}\{#MyAppName}

; Ausgabe-Einstellungen
OutputDir=..\build3\Installer
OutputBaseFilename=Aura_Setup_v{#MyAppVersion}_Win64
SetupIconFile=..\Resources\icon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName} {#MyAppVersion}

; Kompression
Compression=lzma2/ultra64
SolidCompression=yes
LZMAUseSeparateProcess=yes

; Installer-Look
WizardStyle=modern
WizardSizePercent=120,120
WizardImageFile=WizardImage.bmp
WizardSmallImageFile=WizardSmallImage.bmp

; Rechte - Standard: Admin, aber User darf override
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
UsedUserAreasWarning=no

; Weitere Einstellungen
DisableProgramGroupPage=yes
DisableWelcomePage=no
LicenseFile=..\LICENSE.txt
InfoBeforeFile=..\README_INSTALL.txt
ShowLanguageDialog=auto
AllowNoIcons=yes
ChangesAssociations=no
CloseApplications=yes
RestartApplications=no
MinVersion=10.0

; Architektur
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible

; Uninstall-Info
CreateUninstallRegKey=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

; ============================================================================
; Installationstypen
; ============================================================================
[Types]
Name: "full"; Description: "Full Installation (Standalone + VST3 + CLAP)"
Name: "vst3only"; Description: "VST3 Plugin Only"
Name: "claponly"; Description: "CLAP Plugin Only"
Name: "standaloneonly"; Description: "Standalone Application Only"
Name: "custom"; Description: "Custom Installation"; Flags: iscustom

; ============================================================================
; Komponenten-Auswahl
; ============================================================================
[Components]
Name: "standalone"; Description: "Aura Standalone Application"; Types: full standaloneonly custom
Name: "vst3"; Description: "Aura VST3 Plugin"; Types: full vst3only custom
Name: "clap"; Description: "Aura CLAP Plugin (next-gen plugin format)"; Types: full claponly custom
Name: "docs"; Description: "Documentation and README"; Types: full custom; Flags: disablenouninstallwarning

; ============================================================================
; Tasks (optionale Aktionen)
; ============================================================================
[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Components: standalone

; ============================================================================
; Verzeichnisse
; ============================================================================
[Dirs]
; VST3 Verzeichnis erstellen
Name: "{code:GetVST3Dir}"; Components: vst3
; CLAP Verzeichnis erstellen
Name: "{code:GetCLAPDir}"; Components: clap
; Settings-Verzeichnis
Name: "{userappdata}\Unproved Audio\Aura"; Flags: uninsalwaysuninstall

; ============================================================================
; Dateien
; ============================================================================
[Files]
; --- Standalone Application ---
Source: "..\build3\Aura_artefacts\Release\Standalone\Aura.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

; --- VST3 Plugin (ganzer .vst3 Bundle-Ordner) ---
Source: "..\build3\Aura_artefacts\Release\VST3\Aura.vst3\*"; DestDir: "{code:GetVST3Dir}\Aura.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

; --- CLAP Plugin ---
Source: "..\build3\Aura_artefacts\Release\CLAP\Aura.clap"; DestDir: "{code:GetCLAPDir}"; Components: clap; Flags: ignoreversion

; --- Icon ---
Source: "..\Resources\icon.ico"; DestDir: "{app}"; Flags: ignoreversion

; --- Dokumentation ---
Source: "..\README_INSTALL.txt"; DestDir: "{app}"; DestName: "README.txt"; Components: docs; Flags: ignoreversion
Source: "..\LICENSE.txt"; DestDir: "{app}"; Components: docs; Flags: ignoreversion

; ============================================================================
; Icons / Shortcuts
; ============================================================================
[Icons]
; Start Menu
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\icon.ico"; Components: standalone
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
; Desktop (optional)
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\icon.ico"; Tasks: desktopicon; Components: standalone

; ============================================================================
; Registry (fuer Deinstallation und Versionserkennung)
; ============================================================================
[Registry]
Root: HKLM; Subkey: "Software\{#MyAppPublisher}\{#MyAppName}"; ValueType: string; ValueName: "InstallDir"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\{#MyAppPublisher}\{#MyAppName}"; ValueType: string; ValueName: "Version"; ValueData: "{#MyAppVersion}"; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\{#MyAppPublisher}\{#MyAppName}"; ValueType: string; ValueName: "VST3Dir"; ValueData: "{code:GetVST3Dir}"; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\{#MyAppPublisher}\{#MyAppName}"; ValueType: string; ValueName: "CLAPDir"; ValueData: "{code:GetCLAPDir}"; Flags: uninsdeletekey

; ============================================================================
; Nach Installation starten
; ============================================================================
[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent; Components: standalone

; ============================================================================
; Deinstallation
; ============================================================================
[UninstallDelete]
Type: filesandordirs; Name: "{code:GetVST3Dir}\Aura.vst3"
Type: files; Name: "{code:GetCLAPDir}\Aura.clap"
Type: filesandordirs; Name: "{userappdata}\Unproved Audio\Aura"
Type: dirifempty; Name: "{userappdata}\Unproved Audio"
Type: dirifempty; Name: "{app}"

; ============================================================================
; Pascal Script fuer benutzerdefinierte Seiten
; ============================================================================
[Code]
var
  VST3DirPage: TInputDirWizardPage;
  CLAPDirPage: TInputDirWizardPage;

// Standard VST3 Pfad ermitteln
function GetDefaultVST3Dir(): String;
var
  RegValue: String;
begin
  // Versuche zuerst aus der Registry zu lesen (vorheriger Install-Pfad)
  if RegQueryStringValue(HKLM, 'Software\{#MyAppPublisher}\{#MyAppName}', 'VST3Dir', RegValue) then
  begin
    if DirExists(RegValue) then
    begin
      Result := RegValue;
      Exit;
    end;
  end;
  // Standard-VST3-Pfad
  Result := ExpandConstant('{commonpf}\Common Files\VST3');
end;

// Standard CLAP Pfad ermitteln
function GetDefaultCLAPDir(): String;
var
  RegValue: String;
begin
  if RegQueryStringValue(HKLM, 'Software\{#MyAppPublisher}\{#MyAppName}', 'CLAPDir', RegValue) then
  begin
    if DirExists(RegValue) then
    begin
      Result := RegValue;
      Exit;
    end;
  end;
  // Standard-CLAP-Pfad
  Result := ExpandConstant('{commonpf}\Common Files\CLAP');
end;

// VST3 Pfad aus Benutzerauswahl
function GetVST3Dir(Param: String): String;
begin
  if (VST3DirPage <> nil) and (VST3DirPage.Values[0] <> '') then
    Result := VST3DirPage.Values[0]
  else
    Result := GetDefaultVST3Dir();
end;

// CLAP Pfad aus Benutzerauswahl
function GetCLAPDir(Param: String): String;
begin
  if (CLAPDirPage <> nil) and (CLAPDirPage.Values[0] <> '') then
    Result := CLAPDirPage.Values[0]
  else
    Result := GetDefaultCLAPDir();
end;

// Wizard-Seiten erstellen
procedure InitializeWizard();
begin
  // --- VST3-Pfad-Auswahl Seite ---
  VST3DirPage := CreateInputDirPage(wpSelectComponents,
    'VST3 Plugin Location',
    'Where should the VST3 plugin be installed?',
    'Select the folder where the VST3 plugin should be installed.' + #13#10 + #13#10 +
    'The standard VST3 folder is detected automatically. Most DAWs (Ableton Live, ' +
    'FL Studio, Cubase, Studio One, Reaper, etc.) scan this folder automatically.' + #13#10 + #13#10 +
    'Only change this if your DAW uses a custom VST3 folder.',
    False, 'New Folder');

  VST3DirPage.Add('VST3 Plugin Folder:');
  VST3DirPage.Values[0] := GetDefaultVST3Dir();

  // --- CLAP-Pfad-Auswahl Seite ---
  CLAPDirPage := CreateInputDirPage(VST3DirPage.ID,
    'CLAP Plugin Location',
    'Where should the CLAP plugin be installed?',
    'Select the folder where the CLAP plugin should be installed.' + #13#10 + #13#10 +
    'The standard CLAP folder is detected automatically. DAWs like Bitwig, Reaper, ' +
    'and others supporting the CLAP format scan this folder by default.' + #13#10 + #13#10 +
    'Only change this if your DAW uses a custom CLAP folder.',
    False, 'New Folder');

  CLAPDirPage.Add('CLAP Plugin Folder:');
  CLAPDirPage.Values[0] := GetDefaultCLAPDir();
end;

// VST3/CLAP-Seite nur anzeigen wenn jeweilige Komponente ausgewaehlt
function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if (VST3DirPage <> nil) and (PageID = VST3DirPage.ID) then
    Result := not WizardIsComponentSelected('vst3');
  if (CLAPDirPage <> nil) and (PageID = CLAPDirPage.ID) then
    Result := not WizardIsComponentSelected('clap');
end;

// Pruefen ob bereits installiert
function IsPreviousVersionInstalled(): Boolean;
begin
  Result := RegKeyExists(HKLM, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{#SetupSetting("AppId")}_is1');
end;

// Vor der Installation
function InitializeSetup(): Boolean;
var
  PrevVersion: String;
begin
  Result := True;
  // Warnung wenn bereits installiert
  if IsPreviousVersionInstalled() then
  begin
    RegQueryStringValue(HKLM, 'Software\{#MyAppPublisher}\{#MyAppName}', 'Version', PrevVersion);
    if PrevVersion = '' then PrevVersion := 'unknown';
    if MsgBox('Aura v' + PrevVersion + ' is already installed.' + #13#10 + #13#10 +
              'Do you want to update to version {#MyAppVersion}?',
              mbConfirmation, MB_YESNO) = IDNO then
      Result := False;
  end;
end;

// DAW-Warnung wenn VST3 installiert wird
procedure CurStepChanged(CurStep: TSetupStep);
var
  Msg: String;
begin
  if CurStep = ssPostInstall then
  begin
    Msg := '';
    if WizardIsComponentSelected('vst3') then
      Msg := 'VST3 Location: ' + GetVST3Dir('') + '\Aura.vst3' + #13#10;
    if WizardIsComponentSelected('clap') then
      Msg := Msg + 'CLAP Location: ' + GetCLAPDir('') + '\Aura.clap' + #13#10;
    
    if Msg <> '' then
    begin
      MsgBox('Plugins installed successfully!' + #13#10 + #13#10 +
             'Please restart your DAW and rescan your plugins to detect Aura.' + #13#10 + #13#10 +
             Msg,
             mbInformation, MB_OK);
    end;
  end;
end;

[Messages]
; Deutsche Anpassungen
german.WelcomeLabel1=Willkommen bei der Installation von [name]
german.WelcomeLabel2=Dieses Programm installiert [name/ver] auf Ihrem Computer.%n%nAura ist ein professioneller parametrischer Equalizer von Unproved Audio.%n%nEs wird empfohlen, alle DAWs zu schliessen, bevor Sie fortfahren.
