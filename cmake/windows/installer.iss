; Windows installer for LiveSubtitles, built with Inno Setup 6: see docs/development.md.
; It expects the plugin installed in release\staging by "cmake --install".

#ifndef Version
  #define Version "0.2.0"
#endif
#define Repo SourcePath + "..\..\"
#define Staging Repo + "release\staging\livesubtitles"
#define Deps Repo + "build_x64\_deps"
; The translation models are downloaded from the models-v1 release of the repository,
; only for the languages ticked. Keep the hashes in line with CMakeLists.txt.
#define Models "https://github.com/ValotKzm/livesubtitles/releases/download/models-v1/"

[Setup]
AppId={{E61438F8-857B-4815-90AB-42B5FB41D9C9}
AppName=LiveSubtitles
AppVersion={#Version}
AppPublisher=LiveSubtitles contributors
AppPublisherURL=https://github.com/ValotKzm/livesubtitles
; OBS Studio loads the plugins of every user from this folder.
DefaultDirName={commonappdata}\obs-studio\plugins\livesubtitles
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayName=LiveSubtitles (OBS Studio plugin)
OutputDir={#Repo}release
OutputBaseFilename=LiveSubtitles-{#Version}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; Needed to extract the .zip archives of the translation models.
ArchiveExtraction=full

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Messages]
english.FinishedLabel=LiveSubtitles is installed. Start OBS Studio, add a "LiveSubtitles" source to your scene, choose your microphone and turn the subtitles on.
french.FinishedLabel=LiveSubtitles est installé. Lancez OBS Studio, ajoutez une source « LiveSubtitles » à votre scène, choisissez votre microphone et activez les sous-titres.
english.SelectComponentsDesc=Which languages will you speak or want subtitles in?
french.SelectComponentsDesc=Quelles langues parlerez-vous ou voudrez-vous en sous-titres ?
english.SelectComponentsLabel2=Tick every language you will speak or want subtitles in. Setup downloads the translation model of each pair of ticked languages (about 65 MB each), so an Internet connection is needed. You can run Setup again later to change the languages.
french.SelectComponentsLabel2=Cochez chaque langue que vous parlerez ou voudrez en sous-titres. L'installation télécharge le modèle de traduction de chaque paire de langues cochées (environ 65 Mo chacun) : une connexion Internet est nécessaire. Vous pourrez relancer l'installation plus tard pour changer de langues.

[CustomMessages]
english.TypeLanguages=Chosen languages
french.TypeLanguages=Langues choisies
english.LanguageFrench=French
french.LanguageFrench=Français
english.LanguageEnglish=English
french.LanguageEnglish=Anglais
english.LanguageRussian=Russian
french.LanguageRussian=Russe
english.NoLanguage=Tick at least one language.
french.NoLanguage=Cochez au moins une langue.

[Types]
Name: "languages"; Description: "{cm:TypeLanguages}"; Flags: iscustom

[Components]
Name: "fr"; Description: "{cm:LanguageFrench}"; Types: languages
Name: "en"; Description: "{cm:LanguageEnglish}"; Types: languages
Name: "ru"; Description: "{cm:LanguageRussian}"; Types: languages

[Dirs]
; The plugin only offers the languages that have a folder here.
Name: "{app}\data\languages\fr"; Components: fr
Name: "{app}\data\languages\en"; Components: en
Name: "{app}\data\languages\ru"; Components: ru

[InstallDelete]
; Running Setup again with fewer languages removes the models of the others.
Type: filesandordirs; Name: "{app}\data\languages\fr"; Components: not fr
Type: filesandordirs; Name: "{app}\data\models\opus-mt-fr-*"; Components: not fr
Type: filesandordirs; Name: "{app}\data\models\opus-mt-*-fr"; Components: not fr
Type: filesandordirs; Name: "{app}\data\languages\en"; Components: not en
Type: filesandordirs; Name: "{app}\data\models\opus-mt-en-*"; Components: not en
Type: filesandordirs; Name: "{app}\data\models\opus-mt-*-en"; Components: not en
Type: filesandordirs; Name: "{app}\data\languages\ru"; Components: not ru
Type: filesandordirs; Name: "{app}\data\models\opus-mt-ru-*"; Components: not ru
Type: filesandordirs; Name: "{app}\data\models\opus-mt-*-ru"; Components: not ru

[Files]
Source: "{#Staging}\bin\64bit\livesubtitles.dll"; DestDir: "{app}\bin\64bit"; Flags: ignoreversion
Source: "{#Staging}\data\*"; Excludes: "opus-mt-*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs
Source: "{#Models}opus-mt-fr-en-ct2-int8.zip"; DestName: "opus-mt-fr-en-ct2-int8.zip"; DestDir: "{app}\data\models\opus-mt-fr-en"; ExternalSize: 63481302; Hash: "281c06fa89a1f0a3bff47ffc3445fc5087c54bfbb50ed37816240a0b9c902b1f"; Components: fr and en; Flags: external download extractarchive recursesubdirs createallsubdirs ignoreversion
Source: "{#Models}opus-mt-en-fr-ct2-int8.zip"; DestName: "opus-mt-en-fr-ct2-int8.zip"; DestDir: "{app}\data\models\opus-mt-en-fr"; ExternalSize: 62400854; Hash: "b852892f3f2cf862f42a16894cecedc8c368e29f153a123d33538a4ce7d7b4fe"; Components: en and fr; Flags: external download extractarchive recursesubdirs createallsubdirs ignoreversion
Source: "{#Models}opus-mt-fr-ru-ct2-int8.zip"; DestName: "opus-mt-fr-ru-ct2-int8.zip"; DestDir: "{app}\data\models\opus-mt-fr-ru"; ExternalSize: 70933230; Hash: "94f7302bc0ef4db6f7f84cc71c7152edacb32320db0f6b2210b4bb82dbbd3eb7"; Components: fr and ru; Flags: external download extractarchive recursesubdirs createallsubdirs ignoreversion
Source: "{#Models}opus-mt-en-ru-ct2-int8.zip"; DestName: "opus-mt-en-ru-ct2-int8.zip"; DestDir: "{app}\data\models\opus-mt-en-ru"; ExternalSize: 69576817; Hash: "a1cff77d5ee3d368ede58073513f4eca8c283a3fd49d354be98412b1e9b57b4f"; Components: en and ru; Flags: external download extractarchive recursesubdirs createallsubdirs ignoreversion
Source: "{#Models}opus-mt-ru-en-ct2-int8.zip"; DestName: "opus-mt-ru-en-ct2-int8.zip"; DestDir: "{app}\data\models\opus-mt-ru-en"; ExternalSize: 67479338; Hash: "86abffec4bfce3532a09c09b9a73c5a292db8a7c2ac84cb0ce20876531718571"; Components: ru and en; Flags: external download extractarchive recursesubdirs createallsubdirs ignoreversion
Source: "{#Models}opus-mt-ru-fr-ct2-int8.zip"; DestName: "opus-mt-ru-fr-ct2-int8.zip"; DestDir: "{app}\data\models\opus-mt-ru-fr"; ExternalSize: 65552119; Hash: "589e590d93ea07592d2a1fd41c5968d3f7460934c79a8ea645fbab1898df20ef"; Components: ru and fr; Flags: external download extractarchive recursesubdirs createallsubdirs ignoreversion
Source: "{#Repo}LICENSE"; DestDir: "{app}\licenses"; DestName: "LiveSubtitles-GPL-2.0.txt"; Flags: ignoreversion
Source: "{#SourcePath}resources\GPL-3.0.txt"; DestDir: "{app}\licenses"; Flags: ignoreversion
Source: "{#SourcePath}resources\third-party-notices.txt"; DestDir: "{app}\licenses"; Flags: ignoreversion
Source: "{#Deps}\whisper-src\LICENSE"; DestDir: "{app}\licenses"; DestName: "whisper.cpp.txt"; Flags: ignoreversion
Source: "{#Deps}\ctranslate2-src\LICENSE"; DestDir: "{app}\licenses"; DestName: "CTranslate2.txt"; Flags: ignoreversion
Source: "{#Deps}\ctranslate2-src\third_party\ruy\LICENSE"; DestDir: "{app}\licenses"; DestName: "ruy.txt"; Flags: ignoreversion
Source: "{#Deps}\ctranslate2-src\third_party\ruy\third_party\cpuinfo\LICENSE"; DestDir: "{app}\licenses"; DestName: "cpuinfo.txt"; Flags: ignoreversion
Source: "{#Deps}\ctranslate2-src\third_party\cpu_features\LICENSE"; DestDir: "{app}\licenses"; DestName: "cpu_features.txt"; Flags: ignoreversion
Source: "{#Deps}\ctranslate2-src\third_party\spdlog\LICENSE"; DestDir: "{app}\licenses"; DestName: "spdlog.txt"; Flags: ignoreversion
Source: "{#Deps}\sentencepiece-src\LICENSE"; DestDir: "{app}\licenses"; DestName: "SentencePiece.txt"; Flags: ignoreversion
Source: "{#Deps}\sentencepiece-src\third_party\absl\LICENSE"; DestDir: "{app}\licenses"; DestName: "abseil.txt"; Flags: ignoreversion
Source: "{#Deps}\sentencepiece-src\third_party\darts_clone\LICENSE"; DestDir: "{app}\licenses"; DestName: "darts-clone.txt"; Flags: ignoreversion
Source: "{#Deps}\sentencepiece-src\third_party\esaxx\LICENSE"; DestDir: "{app}\licenses"; DestName: "esaxx.txt"; Flags: ignoreversion
Source: "{#Deps}\sentencepiece-src\third_party\protobuf-lite\LICENSE"; DestDir: "{app}\licenses"; DestName: "protobuf.txt"; Flags: ignoreversion

[Code]
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = wpSelectComponents) and (WizardSelectedComponents(False) = '') then begin
    MsgBox(CustomMessage('NoLanguage'), mbError, MB_OK);
    Result := False;
  end;
end;
