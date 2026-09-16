#ifndef MyAppVersion
  #define MyAppVersion "0.0.0-dev"
#endif
#ifndef MySourceDir
  #define MySourceDir "..\package"
#endif
#ifndef MyOutputDir
  #define MyOutputDir ".."
#endif
#ifndef MySetupIcon
  #define MySetupIcon "..\package\superpower.ico"
#endif

[Setup]
AppId={{8F01EF00-467A-4F0F-98CB-B5643B71F532}
AppName=Superpower Desktop
AppVersion={#MyAppVersion}
AppVerName=Superpower Desktop {#MyAppVersion}
AppPublisher=Superpower
AppPublisherURL=https://github.com/stloendays/Superpower
AppSupportURL=https://github.com/stloendays/Superpower/issues
AppUpdatesURL=https://github.com/stloendays/Superpower/releases/latest
DefaultDirName={localappdata}\Programs\Superpower Desktop
DefaultGroupName=Superpower
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#MyOutputDir}
OutputBaseFilename=Superpower-Desktop-{#MyAppVersion}-Setup
SetupIconFile={#MySetupIcon}
UninstallDisplayIcon={app}\superpower.ico
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
SetupLogging=yes

[Files]
Source: "{#MySourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Superpower Desktop"; Filename: "{app}\Superpower Desktop.exe"; WorkingDir: "{app}"; IconFilename: "{app}\superpower.ico"; Comment: "Superpower MCP Desktop"
Name: "{autodesktop}\Superpower Desktop"; Filename: "{app}\Superpower Desktop.exe"; WorkingDir: "{app}"; IconFilename: "{app}\superpower.ico"; Comment: "Superpower MCP Desktop"

[Run]
Filename: "{app}\Superpower Desktop.exe"; Description: "Launch Superpower Desktop"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent
Filename: "{app}\Superpower Desktop.exe"; WorkingDir: "{app}"; Flags: nowait; Check: WizardSilent

[UninstallDelete]
Type: files; Name: "{app}\.superpower-installed"

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    SaveStringToFile(ExpandConstant('{app}\.superpower-installed'),
      'version={#MyAppVersion}' + #13#10, False);
end;
