#define MyAppName "Uboot"
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "."
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif

[Setup]
AppId={{D2B03F7B-ACD1-44A0-8F43-1EE26DBE3A27}
AppName={#MyAppName}
AppVersion={#AppVersion}
AppPublisher=Uboot
AppPublisherURL=https://github.com/ezesc/Uboot
AppSupportURL=https://github.com/ezesc/Uboot/issues
AppUpdatesURL=https://github.com/ezesc/Uboot/releases
DefaultDirName={autopf}\Uboot
DefaultGroupName=Uboot
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
OutputDir={#OutputDir}
OutputBaseFilename=Uboot-Setup-{#AppVersion}
UninstallDisplayIcon={app}\Uboot.exe

[Types]
Name: "full"; Description: "Full installation"

[Components]
Name: "main"; Description: "Uboot application"; Types: full; Flags: fixed
Name: "ai"; Description: "Local AI component (Better mode, offline-ready)"; Types: full

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Excludes: "llm\*"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: main
Source: "{#SourceDir}\llm\*"; DestDir: "{app}\llm"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: ai

[Icons]
Name: "{autoprograms}\Uboot"; Filename: "{app}\Uboot.exe"
Name: "{autodesktop}\Uboot"; Filename: "{app}\Uboot.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Uboot.exe"; Description: "Launch Uboot"; Flags: nowait postinstall skipifsilent
