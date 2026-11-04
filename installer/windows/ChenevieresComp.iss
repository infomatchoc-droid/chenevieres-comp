; Installateur Windows de Chenevieres Comp (Inno Setup 6)

#define AppName "Chenevieres Comp"
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

[Setup]
AppId={{5152BF8D-DEB1-4CCB-8DDC-AF54A05FB3E8}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Studio des Chenevieres
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputBaseFilename=Chenevieres-Comp-{#AppVersion}-Windows
OutputDir=..\..\dist
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; Habillage (2.1) : bandeau, vignette et icône aux couleurs du studio (installer/branding)
WizardImageFile=..\branding\wizard-large-100.bmp,..\branding\wizard-large-125.bmp,..\branding\wizard-large-150.bmp,..\branding\wizard-large-200.bmp
WizardSmallImageFile=..\branding\wizard-small-100.bmp,..\branding\wizard-small-125.bmp,..\branding\wizard-small-150.bmp,..\branding\wizard-small-200.bmp
WizardImageStretch=no
SetupIconFile=chenevieres.ico
UninstallDisplayIcon={uninstallexe}
ShowLanguageDialog=auto
UninstallDisplayName={#AppName} (VST3, AAX)
UninstallFilesDir={commonpf64}\Studio des Chenevieres\{#AppName}

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
french.WelcomeLabel1=Bienvenue dans l'installation de Chènevières Comp
french.WelcomeLabel2=Merci d'avoir choisi Chènevières Comp, le compresseur multi-modèles du Studio des Chènevières.%n%nL'installation copie le plugin VST3 (et AAX pour Pro Tools). Fermez votre logiciel audio avant de continuer.%n%nCliquez sur Suivant pour commencer.
french.FinishedHeadingLabel=C'est installé, merci !
french.FinishedLabel=Chènevières Comp est prêt. Ouvrez votre logiciel audio et relancez la recherche des plugins si besoin.%n%nBonne musique,%nMathieu — Studio des Chènevières
english.WelcomeLabel1=Welcome to the Chènevières Comp setup
english.WelcomeLabel2=Thank you for choosing Chènevières Comp, the multi-model compressor from Studio des Chènevières.%n%nSetup installs the VST3 plug-in (and AAX for Pro Tools). Please close your audio software before continuing.%n%nClick Next to start.
english.FinishedHeadingLabel=All set, thank you!
english.FinishedLabel=Chènevières Comp is ready. Open your audio software and rescan your plug-ins if needed.%n%nEnjoy the music,%nMathieu — Studio des Chènevières

[CustomMessages]
french.GiveFeedback=Donner mon avis (2 minutes)
english.GiveFeedback=Give my feedback (2 minutes)
french.DawOpen=Ce logiciel audio est encore ouvert :%n%n%1%nFermez-le, puis cliquez sur Réessayer. Sinon, Windows ne pourra pas remplacer le plugin (erreur « DeleteFile code 5 »).%n%nIgnorer : continuer quand même.
english.DawOpen=This audio software is still open:%n%n%1%nPlease close it, then click Retry. Otherwise Windows cannot replace the plug-in ("DeleteFile code 5" error).%n%nIgnore: continue anyway.

[Components]
Name: "vst3"; Description: "VST3"; Types: full compact custom; Flags: fixed
Name: "aax"; Description: "AAX (Pro Tools)"; Types: full custom

[Files]
Source: "..\..\build\ChenevieresComp_artefacts\Release\VST3\Chenevieres Comp.vst3\*"; DestDir: "{commoncf64}\VST3\Chenevieres Comp.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\build\ChenevieresComp_artefacts\Release\AAX\Chenevieres Comp.aaxplugin\*"; DestDir: "{commoncf64}\Avid\Audio\Plug-Ins\Chenevieres Comp.aaxplugin"; Components: aax; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Chenevieres Comp.vst3"
Type: filesandordirs; Name: "{commoncf64}\Avid\Audio\Plug-Ins\Chenevieres Comp.aaxplugin"

[Run]
Filename: "https://studiodeschenevieres.fr/avis"; Description: "{cm:GiveFeedback}"; Flags: postinstall shellexec skipifsilent nowait

[Code]
// 2.2 : avant la copie, vérifie qu'aucun logiciel audio n'utilise le plugin (sinon « DeleteFile code 5 »)
function RunningDaws(): String;
var
  Locator, Service, Procs, Item: Variant;
  Patterns: array of String;
  I, J: Integer;
  Found: String;
begin
  Result := '';
  SetArrayLength(Patterns, 27);
  Patterns[0] := 'Cubase%';
  Patterns[1] := 'Nuendo%';
  Patterns[2] := 'Dorico%';
  Patterns[3] := 'Ableton Live%';
  Patterns[4] := 'reaper%';
  Patterns[5] := 'Studio One%';
  Patterns[6] := 'FL64%';
  Patterns[7] := 'FL.exe';
  Patterns[8] := 'ProTools%';
  Patterns[9] := 'Pro Tools%';
  Patterns[10] := 'Bitwig Studio%';
  Patterns[11] := 'Resolve%';
  Patterns[12] := 'Adobe Premiere Pro%';
  Patterns[13] := 'Adobe Audition%';
  Patterns[14] := 'Cakewalk%';
  Patterns[15] := 'Waveform%';
  Patterns[16] := 'Pyramix%';
  Patterns[17] := 'Samplitude%';
  Patterns[18] := 'Sequoia%';
  Patterns[19] := 'Audacity%';
  Patterns[20] := 'Reason%';
  Patterns[21] := 'VEPro%';
  Patterns[22] := 'Vienna Ensemble Pro%';
  Patterns[23] := 'Gig Performer%';
  Patterns[24] := 'Cantabile%';
  Patterns[25] := 'Ardour%';
  Patterns[26] := 'Mixbus%';
  try
    Locator := CreateOleObject('WbemScripting.SWbemLocator');
    Service := Locator.ConnectServer('.', 'root\CIMV2');
    for I := 0 to GetArrayLength(Patterns) - 1 do
    begin
      Procs := Service.ExecQuery('SELECT Name FROM Win32_Process WHERE Name LIKE ''' + Patterns[I] + '''');
      if not VarIsNull(Procs) then
        for J := 0 to Procs.Count - 1 do
        begin
          Item := Procs.ItemIndex(J);
          Found := Item.Name;
          if Pos(Found, Result) = 0 then
            Result := Result + '  - ' + Found + #13#10;
        end;
    end;
  except
    Result := '';   // WMI indisponible : on ne bloque pas l'installation
  end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  Daws: String;
  Answer: Integer;
begin
  Result := True;
  if CurPageID = wpReady then
  begin
    Daws := RunningDaws();
    while Daws <> '' do
    begin
      Answer := MsgBox(FmtMessage(CustomMessage('DawOpen'), [Daws]), mbError, MB_ABORTRETRYIGNORE);
      if Answer = IDABORT then
      begin
        Result := False;
        Exit;
      end;
      if Answer = IDIGNORE then
        Exit;
      Daws := RunningDaws();
    end;
  end;
end;
