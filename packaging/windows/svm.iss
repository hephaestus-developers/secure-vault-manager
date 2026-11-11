; Inno Setup Script for Secure Vault Manager
#define MyAppName "Secure Vault Manager"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "Hephaestus Developers"
#define MyAppURL "https://github.com/hephaestus-developers/secure-vault-manager"
#define MyAppExeName "svm.exe"

[Setup]
AppId={{E68F92F1-4B2E-4D78-9584-9B0A33B91A42}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE.LGPLv3
OutputDir=..\..\build\packages
OutputBaseFilename=SecureVaultManager-{#MyAppVersion}-Setup
SetupIconFile=svm.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\..\build\release\ui\svm.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\build\release\core\svm-cli.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\build\release\core\svm-native-host.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\core\target\release\svm_core.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "svm.ico"; DestDir: "{app}"; Flags: ignoreversion
; Native messaging manifest
Source: "..\native-messaging\chrome.json.in"; DestDir: "{app}"; DestName: "io.hephaestus.svm.json"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\svm.ico"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\svm.ico"; Tasks: desktopicon

[Registry]
; Chromium & Chrome Native Messaging Host
Root: HKCU; Subkey: "Software\Google\Chrome\NativeMessagingHosts\io.hephaestus.svm"; ValueType: string; ValueData: "{app}\io.hephaestus.svm.json"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Microsoft\Edge\NativeMessagingHosts\io.hephaestus.svm"; ValueType: string; ValueData: "{app}\io.hephaestus.svm.json"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\BraveSoftware\Brave-Browser\NativeMessagingHosts\io.hephaestus.svm"; ValueType: string; ValueData: "{app}\io.hephaestus.svm.json"; Flags: uninsdeletekey

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
