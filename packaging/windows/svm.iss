; Inno Setup Script for Secure Vault Manager
#define MyAppName "Secure Vault Manager"
#define MyAppVersion "2026.11.0"
#define MyAppPublisher "Hephaestus Developers"
#define MyAppURL "https://github.com/hephaestus-developers/secure-vault-manager"
#define MyAppExeName "svm.exe"

#ifndef SourceBundleDir
  #define SourceBundleDir "..\..\dist\secure-vault-manager-2026.11.0-windows-x64"
#endif

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
OutputDir=..\..\dist
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
; Cài đặt toàn bộ thư mục bundle (đã bao gồm Qt6 DLLs, plugins, svm.exe, svm-cli.exe, svm-native-host.exe, svm_core.dll)
Source: "{#SourceBundleDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "svm.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\svm.ico"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\svm.ico"; Tasks: desktopicon

[Registry]
; Google Chrome Native Messaging Host
Root: HKLM; Subkey: "Software\Google\Chrome\NativeMessagingHosts\io.hephaestus.secure_vault_manager"; ValueType: string; ValueData: "{app}\native-messaging\chrome.json"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Google\Chrome\NativeMessagingHosts\io.hephaestus.secure_vault_manager"; ValueType: string; ValueData: "{app}\native-messaging\chrome.json"; Flags: uninsdeletekey

; Microsoft Edge Native Messaging Host
Root: HKLM; Subkey: "Software\Microsoft\Edge\NativeMessagingHosts\io.hephaestus.secure_vault_manager"; ValueType: string; ValueData: "{app}\native-messaging\chrome.json"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Microsoft\Edge\NativeMessagingHosts\io.hephaestus.secure_vault_manager"; ValueType: string; ValueData: "{app}\native-messaging\chrome.json"; Flags: uninsdeletekey

; Brave Browser Native Messaging Host
Root: HKLM; Subkey: "Software\BraveSoftware\Brave-Browser\NativeMessagingHosts\io.hephaestus.secure_vault_manager"; ValueType: string; ValueData: "{app}\native-messaging\chrome.json"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\BraveSoftware\Brave-Browser\NativeMessagingHosts\io.hephaestus.secure_vault_manager"; ValueType: string; ValueData: "{app}\native-messaging\chrome.json"; Flags: uninsdeletekey

; Mozilla Firefox Native Messaging Host
Root: HKLM; Subkey: "Software\Mozilla\NativeMessagingHosts\io.hephaestus.secure_vault_manager"; ValueType: string; ValueData: "{app}\native-messaging\firefox.json"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Mozilla\NativeMessagingHosts\io.hephaestus.secure_vault_manager"; ValueType: string; ValueData: "{app}\native-messaging\firefox.json"; Flags: uninsdeletekey

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
