; Agape48 - Windows installer (Inno Setup 6)
;
; Built with:
;   python tools\make-icon.py
;   & "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\agape48.iss
;
; Deliberately NOT included: an HP 48 ROM. The app looks for a file called "rom"
; in its state folder (%LOCALAPPDATA%\Agape48\Agape48\) and reports a clear error when
; it is missing, so shipping one is a licensing decision rather than a technical
; need. Keeping it out also means this installer can be handed to anyone.
;
; Not code-signed. On a machine running an EDR agent - CrowdStrike Falcon is on
; this one - an unsigned installer with no reputation is exactly the shape that
; gets quarantined. Authenticode signing with an OV or EV certificate is the fix,
; and it is a purchase, not a build flag. Nothing here is packed or compressed
; beyond Inno's own LZMA: packing an executable is one of the most reliable ways
; to be flagged, because it is what malware does to hide.

#define AppName        "Agape48"
#define AppVersion     "0.1.0"
#define AppPublisher   "Agape48"
#define AppExeName     "agape48.exe"

[Setup]
AppId={{7A4E0C2E-1D3B-4E77-9F1A-AG48EMU00001}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir=.
OutputBaseFilename=Agape48-{#AppVersion}-windows-x64-setup
SetupIconFile=agape48.ico
UninstallDisplayIcon={app}\agape48.ico
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; 64-bit only: the payload is a MinGW x86_64 build against Qt 6.11.1 msvc-free.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Program Files needs elevation; nothing here writes to the user's profile.
PrivilegesRequired=admin
; Refuse to install over a running copy rather than leaving half-replaced DLLs.
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
; The whole deployed payload: the executable, the Qt runtime windeployqt
; selected, the MinGW runtime DLLs, and the QML modules. Everything under
; payload\ recursively.
Source: "payload\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "agape48.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\agape48.ico"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\agape48.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Start {#AppName}"; Flags: nowait postinstall skipifsilent

; No [UninstallDelete] for the state folder. %LOCALAPPDATA%\Agape48\Agape48 holds the
; calculator's memory - the user's own stack, programs and ROM - and an
; uninstaller has no business deleting that. Same for the settings under
; HKCU\Software\Agape48, which are machine-local preferences, not app files.
