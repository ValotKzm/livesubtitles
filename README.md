# LiveSubtitles

LiveSubtitles is an OBS Studio plugin that shows translated subtitles of what you say, live, on your stream. It is free, runs entirely on your computer and needs no account, API key or server.

[Français](#français)

- **Languages:** French, English and Russian, in any direction, or subtitles without translation.
- **Private:** your voice never leaves your computer and nothing is recorded.
- **Requirements:** Windows 10 or 11 (64-bit) and OBS Studio 31.1 or later. The plugin takes about 700 MB on disk and about 500 MB of memory while it listens.

## Install

1. Close OBS Studio.
2. Download `LiveSubtitles-<version>-windows-x64-setup.exe` from the [Releases page](https://github.com/ValotKzm/livesubtitles/releases) and run it.
3. The installer is not signed, so Windows shows a blue "Windows protected your PC" window: click **More info**, then **Run anyway**.

## Use

1. Start OBS Studio. Under **Sources**, click **+** and add a **LiveSubtitles** source.
2. In the window that opens, choose your **Microphone**, the **Spoken language** and the **Subtitle language**.
3. Tick **Enable LiveSubtitles**. The **Status** line shows "Listening" after a few seconds.
4. Speak: the subtitles appear a second or two after each sentence and fade out by themselves.
5. Move and resize the source in your scene like any other source. The text size, text color, background color and background opacity are in the same window.

Your viewers see nothing but the subtitles: the source stays empty while you are silent or when it is turned off. It only shows a message when you need to do something, for example when no microphone is chosen.

LiveSubtitles listens to the microphone you chose for as long as the box is ticked, even when the source is hidden. Untick it to stop listening.

## If something goes wrong

| Message | What to do |
| --- | --- |
| Choose a microphone | Pick your microphone in the source properties. |
| The microphone is unavailable | Check that the microphone is plugged in and works in Windows, then pick it again. |
| A model file is missing, reinstall the plugin | Close OBS Studio and run the installer again. |

The translation is done by a small model on your computer: it is good for everyday speech, not perfect, and it does not know streaming vocabulary well.

## Uninstall

Close OBS Studio, open **Windows Settings > Apps > Installed apps** and uninstall **LiveSubtitles (OBS Studio plugin)**. Your scenes and OBS settings are left untouched.

## License

LiveSubtitles is free software under the GNU General Public License, version 2 or later; see [LICENSE](LICENSE). The installer also contains third-party software and models, listed in [third-party-notices.txt](cmake/windows/resources/third-party-notices.txt). Developers: see [docs/development.md](docs/development.md) (in French) to build the plugin.

---

# Français

LiveSubtitles est un plugin OBS Studio qui affiche en direct, sur votre stream, les sous-titres traduits de ce que vous dites. Il est gratuit, fonctionne entièrement sur votre ordinateur et ne demande ni compte, ni clé API, ni serveur.

- **Langues :** français, anglais et russe, dans tous les sens, ou sous-titres sans traduction.
- **Confidentialité :** votre voix ne quitte jamais votre ordinateur et rien n'est enregistré.
- **Configuration requise :** Windows 10 ou 11 (64 bits) et OBS Studio 31.1 ou plus récent. Le plugin occupe environ 700 Mo sur le disque et environ 500 Mo de mémoire pendant l'écoute.

## Installer

1. Fermez OBS Studio.
2. Téléchargez `LiveSubtitles-<version>-windows-x64-setup.exe` sur la [page Releases](https://github.com/ValotKzm/livesubtitles/releases) et lancez-le.
3. L'installateur n'est pas signé : Windows affiche une fenêtre bleue « Windows a protégé votre ordinateur ». Cliquez sur **Informations complémentaires**, puis sur **Exécuter quand même**.

## Utiliser

1. Lancez OBS Studio. Dans **Sources**, cliquez sur **+** et ajoutez une source **LiveSubtitles**.
2. Dans la fenêtre qui s'ouvre, choisissez votre **Microphone**, la **Langue parlée** et la **Langue des sous-titres**.
3. Cochez **Activer LiveSubtitles**. La ligne **État** indique « À l'écoute » après quelques secondes.
4. Parlez : les sous-titres apparaissent une à deux secondes après chaque phrase et s'effacent d'eux-mêmes.
5. Déplacez et redimensionnez la source dans votre scène comme n'importe quelle autre. La taille du texte, sa couleur, la couleur et l'opacité du fond se règlent dans la même fenêtre.

Vos viewers ne voient que les sous-titres : la source reste vide quand vous ne parlez pas ou quand elle est désactivée. Elle n'affiche un message que si vous devez agir, par exemple quand aucun microphone n'est choisi.

LiveSubtitles écoute le microphone choisi tant que la case est cochée, même quand la source est masquée. Décochez-la pour arrêter l'écoute.

## En cas de problème

| Message | Que faire |
| --- | --- |
| Choisissez un microphone | Choisissez votre microphone dans les propriétés de la source. |
| Le microphone est indisponible | Vérifiez que le microphone est branché et fonctionne dans Windows, puis choisissez-le de nouveau. |
| Un fichier de modèle est introuvable, réinstallez le plugin | Fermez OBS Studio et relancez l'installateur. |

La traduction est faite par un petit modèle sur votre ordinateur : elle convient à la conversation courante, sans être parfaite, et connaît mal le vocabulaire du streaming.

## Désinstaller

Fermez OBS Studio, ouvrez **Paramètres Windows > Applications > Applications installées** et désinstallez **LiveSubtitles (OBS Studio plugin)**. Vos scènes et vos réglages OBS ne sont pas modifiés.

## Licence

LiveSubtitles est un logiciel libre sous licence publique générale GNU, version 2 ou ultérieure; voir [LICENSE](LICENSE). L'installateur contient aussi des logiciels et des modèles tiers, listés dans [third-party-notices.txt](cmake/windows/resources/third-party-notices.txt). Développeurs : voir [docs/development.md](docs/development.md) pour compiler le plugin.
