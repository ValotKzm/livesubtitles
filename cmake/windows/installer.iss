; Windows installer for LiveSubtitles, built with Inno Setup 6: see docs/development.md.
; It expects the plugin installed in release\staging by "cmake --install".

#ifndef Version
  #define Version "0.1.0"
#endif
#define Repo SourcePath + "..\..\"
#define Staging Repo + "release\staging\livesubtitles"
#define Deps Repo + "build_x64\_deps"

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

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Messages]
english.FinishedLabel=LiveSubtitles is installed. Start OBS Studio, add a "LiveSubtitles" source to your scene, choose your microphone and turn the subtitles on.
french.FinishedLabel=LiveSubtitles est installé. Lancez OBS Studio, ajoutez une source « LiveSubtitles » à votre scène, choisissez votre microphone et activez les sous-titres.

[Files]
Source: "{#Staging}\bin\64bit\livesubtitles.dll"; DestDir: "{app}\bin\64bit"; Flags: ignoreversion
Source: "{#Staging}\data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs
Source: "{#Repo}LICENSE"; DestDir: "{app}\licenses"; DestName: "LiveSubtitles-GPL-2.0.txt"; Flags: ignoreversion
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
