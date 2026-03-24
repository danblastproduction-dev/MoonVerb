; MoonVerb Installer - Inno Setup Script
; DeadLoop (c) 2026

#define MyAppName "MoonVerb"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "DeadLoop"
#define MyAppURL "https://deadloop.fr"

[Setup]
AppId={{C9A4B3E2-5F6D-4B0C-9E8A-2D3F4A5B6C7D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
VersionInfoVersion=1.0.0.0
DefaultDirName={commonpf64}\{#MyAppPublisher}\{#MyAppName}
DefaultGroupName={#MyAppPublisher}\{#MyAppName}
OutputDir=Output
OutputBaseFilename=MoonVerb_v{#MyAppVersion}_Setup
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
PrivilegesRequired=admin
DisableProgramGroupPage=yes
UninstallDisplayName={#MyAppName} {#MyAppVersion}
WizardStyle=modern
WizardSizePercent=120
; SetupIconFile=TODO: add MoonVerb icon
MinVersion=10.0
CloseApplications=force
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Files]
; VST3 bundle
Source: "..\build\MoonVerb_artefacts\Release\VST3\MoonVerb.vst3\Contents\x86_64-win\MoonVerb.vst3"; DestDir: "{commoncf64}\VST3\MoonVerb.vst3\Contents\x86_64-win"; Flags: ignoreversion
Source: "..\build\MoonVerb_artefacts\Release\VST3\MoonVerb.vst3\Contents\Resources\moduleinfo.json"; DestDir: "{commoncf64}\VST3\MoonVerb.vst3\Contents\Resources"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\MoonVerb.vst3"
Type: dirifempty; Name: "{commonpf64}\{#MyAppPublisher}"

[Code]
procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpWelcome then
  begin
    WizardForm.WelcomeLabel1.Caption := 'Welcome to MoonVerb';
    WizardForm.WelcomeLabel2.Caption :=
      'This will install MoonVerb v' + '{#MyAppVersion}' + ' on your computer.' + #13#10 + #13#10 +
      'MoonVerb is a free shimmer reverb plugin by DeadLoop. ' +
      '8-line FDN engine, freeze, modulation, and 15 presets.' + #13#10 + #13#10 +
      'The VST3 plugin will be installed to your system VST3 folder.' + #13#10 + #13#10 +
      'Click Next to continue.';
  end;
end;

function InitializeSetup(): Boolean;
var
  InstalledVersion: String;
begin
  Result := True;
  if RegQueryStringValue(HKLM, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{#SetupSetting("AppId")}_is1',
    'DisplayVersion', InstalledVersion) then
  begin
    if CompareStr(InstalledVersion, '{#MyAppVersion}') = 0 then
    begin
      if MsgBox('{#MyAppName} {#MyAppVersion} is already installed.' + #13#10 +
        'Do you want to reinstall it?', mbConfirmation, MB_YESNO) = IDNO then
        Result := False;
    end;
  end;
end;
