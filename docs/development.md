# Développement et validation

## Façon de travailler

1. Examiner le code, les instructions locales, les dépendances et les tests proches de la zone concernée.
2. Formuler le changement minimal qui répond à la tâche; ne pas réécrire l'architecture pour une correction locale.
3. Vérifier les API, versions OBS, bibliothèques et options de build dans les sources disponibles ou la documentation officielle avant de les utiliser.
4. Modifier une étape à la fois, puis lancer le test ou la vérification la plus ciblée disponible. Élargir aux tests/build pertinents si le changement le justifie.
5. Rapporter ce qui a changé, les vérifications réellement exécutées et les limites restantes.

Ne présume pas qu'une commande de build ou qu'une structure de projet existe : découvre les scripts, configurations et instructions présents avant de lancer ou d'ajouter des commandes.

## Dépendances, licences et sécurité

Avant d'ajouter une dépendance, vérifier sa nécessité, maintenance, compatibilité Windows/OBS, licence, taille et dépendances transitives. Vérifier également les licences des modèles redistribués. Ne pas exécuter de code téléchargé arbitrairement; valider autant que possible l'intégrité des fichiers récupérés. Éviter les privilèges administrateur et les modifications système non nécessaires.

Lors d'un choix technique important, consigner la décision, sa raison, les alternatives considérées et les compromis (par exemple STT, traduction ou séparation en worker).

## Décisions techniques

### Cible OBS, langage et build (vérifié le 2026-10-08)

- **Base du projet :** modèle officiel `obsproject/obs-plugintemplate` (C/C++, CMake avec presets). Il télécharge les sources d'OBS et ses dépendances précompilées selon `buildspec.json`, puis compile `libobs`; OBS installé ne fournit ni en-têtes ni bibliothèques d'import.
- **Version de libobs ciblée :** 31.1.1, celle du `buildspec.json` du modèle. `obs_open_module()` (`libobs/obs-module.c`, tag 32.2.2) ne refuse que les modules compilés avec une libobs plus récente (majeur.mineur) que celle d'OBS; un plugin compilé en 31.1 se charge donc dans OBS 31.1 et suivants. Version d'OBS de test : 32.2.2.
- **Chaîne de compilation Windows :** Visual Studio 2022 (générateur `Visual Studio 17 2022`, imposé par `CMakePresets.json`), Windows SDK 10.0.22621, CMake 3.28 minimum (3.30.5 indiqué par le README du modèle).
- **Commandes de build (exécutées avec succès le 2026-10-08, CMake de VS 2022) :**
  - `cmake --preset windows-x64` : télécharge les sources d'OBS et les dépendances dans `.deps/` (hashes vérifiés par `buildspec.json`) et génère `build_x64/`.
  - `cmake --build --preset windows-x64` : produit `build_x64/RelWithDebInfo/livesubtitles.dll`.
  - `cmake --install build_x64 --config RelWithDebInfo` : copie le plugin dans `%ALLUSERSPROFILE%\obs-studio\plugins\livesubtitles\` (préfixe par défaut du modèle, sans droits administrateur). Fermer OBS avant. Pour retirer le plugin, supprimer ce seul dossier `livesubtitles`.
- **Vérification du chargement :** lancer OBS, puis chercher `[livesubtitles]` dans le dernier fichier de `%APPDATA%\obs-studio\logs`; la ligne `plugin loaded successfully` doit y figurer.
- **Licence du plugin :** GPL-2.0-or-later, celle du modèle officiel et de libobs.
- **Alternative écartée :** cibler libobs 32.2. Son `CMakePresets.json` impose `Visual Studio 18 2026` et le SDK 10.0.26100, le modèle officiel ne le prend pas encore en charge, et le plugin ne se chargerait plus dans OBS 31.x. À réévaluer si une API propre à OBS 32 devient nécessaire.
- **Compromis :** les API ajoutées après 31.1 ne sont pas utilisables; la compatibilité réelle avec OBS 31.1 reste à tester avant de l'annoncer.
- **Environnement de développement vérifié :** Visual Studio Build Tools 2022 17.14 (MSVC 14.44), Windows SDK 10.0.22621 et CMake 3.31.6 embarqué dans `Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin` (absent du PATH).

### Pipeline audio et IA (candidats retenus le 2026-10-08, à valider par mesure)

Sources vérifiées : en-têtes et licences des dépôts cités. Aucune de ces briques n'est encore intégrée ni mesurée.

- **Capture audio :** `obs_source_add_audio_capture_callback()` (`libobs/obs.h`, 31.1.1) sur la source audio OBS choisie par l'utilisateur, ajouté à l'activation et retiré à la désactivation. Pas de code de périphérique Windows à maintenir, et l'audio reçu est celui de la source choisie uniquement. Le thread d'appel n'est pas documenté : le callback se limite à copier les échantillons dans le buffer borné. Alternative écartée : capture WASAPI propre au plugin (plus de code et de cas d'erreur, sans bénéfice pour le MVP).
- **STT :** whisper.cpp (MIT, v1.9.5), lié au plugin, CPU par défaut. Entrée 16 kHz mono (`WHISPER_SAMPLE_RATE`), donc rééchantillonnage depuis le format OBS. Tailles de modèles annoncées : base 142 MiB, small 466 MiB, medium 1,5 GiB; variantes quantifiées plus petites. Pas de vrai mode streaming : résultats progressifs par fenêtres glissantes à notre charge.
- **VAD :** Silero VAD (MIT) via l'API intégrée à whisper.cpp (`whisper_vad_*`, dont `whisper_vad_detect_speech_no_reset` pour le flux continu). Évite d'ajouter ONNX Runtime.
- **Traduction :** deux options derrière la même interface, à départager par mesure de latence et de qualité au jalon 5.
  - Option A : tâche `translate` de Whisper (`whisper_full_params.translate`), une seule inférence et aucune dépendance en plus; limitée à une cible anglaise, sans texte français intermédiaire.
  - Option B : transcription française puis CTranslate2 (MIT) avec OPUS-MT fr-en (Apache-2.0, 75 M de paramètres); deux inférences et une dépendance en plus, mais extensible à d'autres langues cibles.
- **Décision du 2026-10-08 :** démarrer avec l'option A, en gardant l'interface compatible avec l'option B; à confirmer par mesure au jalon 5.
- **Projet existant comparable :** `royshil/obs-localvocal` (GPL-2.0) : filtre audio OBS fondé sur whisper.cpp et CTranslate2, avec de nombreuses options et plusieurs variantes d'installateur. Référence utile pour l'intégration; LiveSubtitles s'en distingue par un parcours unique sans réglage technique.

## Tests et performance

Couvrir les composants au fur et à mesure de leur implémentation : buffer circulaire (écrasement, lecture, reset), VAD (silence/voix/transitions), transcription et traduction, moteur de sous-titres (provisoire/final, remplacement, expiration, découpage) et pipeline intégré. Réutiliser les tests et outils existants; ne pas créer une infrastructure de test spéculative.

Pour les essais audio manuels, varier microphones, silence/bruit, voix et durées. Surveiller stabilité, mémoire, CPU/GPU et latence. Ne pas qualifier le système de « temps réel » sans mesure reproductible.

Compiler et tester à chaque étape pertinente. Après une modification, privilégier d'abord une vérification ciblée; ne pas prétendre qu'un test a réussi s'il n'a pas été exécuté.

Pour l'intégration OBS, vérifier au minimum l'ajout, l'activation/désactivation, la suppression de la source, le déchargement du plugin et la fermeture d'OBS. Confirmer que la capture s'arrête lorsqu'elle est désactivée et qu'aucun worker/callback n'accède à des ressources détruites. Vérifier aussi que les scènes, profils et réglages OBS non détenus par LiveSubtitles restent inchangés. Utiliser la version OBS et les outils réellement documentés pour la cible; signaler les vérifications qui nécessitent un test manuel.

Pour l'installation Windows, prévoir une entrée de désinstallation. Le désinstalleur ne supprime que les fichiers appartenant à LiveSubtitles et ne modifie pas les scènes, profils ou réglages OBS. La suppression des modèles ou réglages propres à l'utilisateur doit être distincte et explicitement choisie. Si une installation manuelle est prise en charge, documenter comment retirer uniquement les fichiers du plugin. Tester l'installation puis la désinstallation avec OBS fermé et vérifier qu'aucun fichier tiers n'est supprimé.

## Documentation et collaboration

Garder le README compréhensible par des streamers non développeurs : fonctionnalités, prérequis, installation, utilisation, modèles, dépannage, confidentialité et licence. Documenter les décisions techniques importantes près de leur domaine.

Quand des commits sont demandés, utiliser des messages descriptifs et cohérents (par exemple `feat: add circular audio buffer`, `fix: handle microphone disconnect`, `test: cover subtitle expiration`). Ne pas créer de commit sans demande explicite.
