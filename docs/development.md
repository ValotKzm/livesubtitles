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
  - `ctest --test-dir build_x64 -C RelWithDebInfo --output-on-failure` : exécute les tests (`tests/`, indépendants d'OBS; option CMake `ENABLE_TESTS`) : tampon circulaire, transitions silence/parole, modèle VAD réel sur du silence et sur un enregistrement de référence, et transcription réelle de ce même enregistrement (anglais). Aucun test automatisé ne couvre encore du français.
  - `build_x64\RelWithDebInfo\transcriber-bench.exe <modèle> <wav> fr 1 4` : mesure un modèle sur un enregistrement 16 kHz mono 16 bits (texte et temps de trois exécutions). `ctest.exe` est dans le même dossier que le CMake de VS 2022.
- **Vérification du chargement :** lancer OBS, puis chercher `[livesubtitles]` dans le dernier fichier de `%APPDATA%\obs-studio\logs`; la ligne `plugin loaded successfully` doit y figurer.
- **Licence du plugin :** GPL-2.0-or-later, celle du modèle officiel et de libobs.
- **Alternative écartée :** cibler libobs 32.2. Son `CMakePresets.json` impose `Visual Studio 18 2026` et le SDK 10.0.26100, le modèle officiel ne le prend pas encore en charge, et le plugin ne se chargerait plus dans OBS 31.x. À réévaluer si une API propre à OBS 32 devient nécessaire.
- **Compromis :** les API ajoutées après 31.1 ne sont pas utilisables; la compatibilité réelle avec OBS 31.1 reste à tester avant de l'annoncer.
- **Environnement de développement vérifié :** Visual Studio Build Tools 2022 17.14 (MSVC 14.44), Windows SDK 10.0.22621 et CMake 3.31.6 embarqué dans `Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin` (absent du PATH).

### Pipeline audio et IA (candidats retenus le 2026-10-08, à valider par mesure)

Sources vérifiées : en-têtes et licences des dépôts cités. Aucune de ces briques n'est encore intégrée ni mesurée.

- **Capture audio :** `obs_source_add_audio_capture_callback()` (`libobs/obs.h`, 31.1.1) sur la source audio OBS choisie par l'utilisateur, ajouté à l'activation et retiré à la désactivation. Pas de code de périphérique Windows à maintenir, et l'audio reçu est celui de la source choisie uniquement. Le thread d'appel n'est pas documenté : le callback se limite à copier les échantillons dans le buffer borné. Alternative écartée : capture WASAPI propre au plugin (plus de code et de cas d'erreur, sans bénéfice pour le MVP).
- **Intégration de whisper.cpp (faite le 2026-10-08) :** v1.9.5 récupérée par `FetchContent` dans `CMakeLists.txt` (archive du tag, SHA-256 vérifié), compilée en bibliothèque statique et liée dans `livesubtitles.dll` (environ 1,3 Mo, aucune DLL supplémentaire à livrer). Réglages : `GGML_NATIVE=OFF` pour ne pas dépendre du processeur de la machine de build, `GGML_OPENMP=OFF` pour ne pas exiger `vcomp140.dll`. Le preset donne à CMake une plateforme `x64,version=...` que ggml ne reconnaît pas : sans correction il compile du code générique sans AVX2, environ dix fois plus lent (constaté : 4,8 s au lieu de 0,5 s par phrase). `CMakeLists.txt` lui passe donc `x64` seul et refuse de configurer si le mode générique est détecté. Limite connue : ce réglage suppose un processeur avec AVX2; le comportement sur un processeur plus ancien n'est pas testé et devra être traité avant distribution.
- **Modèle VAD :** `ggml-silero-v6.2.0.bin` (885 Ko, MIT, dépôt Hugging Face `ggml-org/whisper-vad`), téléchargé à la configuration CMake dans `data/models/` avec SHA-256 vérifié, ignoré par git et installé avec les données du plugin.
- **Mesure VAD (i7-13700KF, un thread CPU) :** environ 0,3 ms de calcul par fenêtre de 32 ms sur l'enregistrement de référence `samples/jfk.wav` de whisper.cpp (test `voice-activity`).
- **STT :** whisper.cpp (MIT, v1.9.5), lié au plugin, CPU par défaut. Entrée 16 kHz mono (`WHISPER_SAMPLE_RATE`), donc rééchantillonnage depuis le format OBS. Tailles de modèles annoncées : base 142 MiB, small 466 MiB, medium 1,5 GiB; variantes quantifiées plus petites. Pas de vrai mode streaming : résultats progressifs par fenêtres glissantes à notre charge.
- **VAD :** Silero VAD (MIT) via l'API intégrée à whisper.cpp (`whisper_vad_*`, dont `whisper_vad_detect_speech_no_reset` pour le flux continu). Évite d'ajouter ONNX Runtime.
- **Traduction :** deux options derrière la même interface, à départager par mesure de latence et de qualité au jalon 5.
  - Option A : tâche `translate` de Whisper (`whisper_full_params.translate`), une seule inférence et aucune dépendance en plus; limitée à une cible anglaise, sans texte français intermédiaire.
  - Option B : transcription française puis CTranslate2 (MIT) avec OPUS-MT fr-en (Apache-2.0, 75 M de paramètres); deux inférences et une dépendance en plus, mais extensible à d'autres langues cibles.
- **Décision du 2026-10-08 :** démarrer avec l'option A, en gardant l'interface compatible avec l'option B; à confirmer par mesure au jalon 5.
- **Projet existant comparable :** `royshil/obs-localvocal` (GPL-2.0) : filtre audio OBS fondé sur whisper.cpp et CTranslate2, avec de nombreuses options et plusieurs variantes d'installateur. Référence utile pour l'intégration; LiveSubtitles s'en distingue par un parcours unique sans réglage technique.

### Transcription et traduction : mesures du 2026-10-08

Conditions : i7-13700KF, CPU seul, 4 threads, tâche `translate` français vers anglais, outil `transcriber-bench` (mêmes options de compilation que le plugin). Audio : neuf phrases de 2 à 15 secondes produites par la synthèse vocale Windows (voix Hortense), donc une diction propre et sans bruit : ces chiffres sont optimistes pour la qualité et ne remplacent pas un essai à la voix. Les enregistrements ne sont pas dans le dépôt.

| Modèle | Taille | Temps par phrase | Qualité observée |
|---|---|---|---|
| `ggml-base-q5_1` | 57 Mo | 0,51 à 0,65 s | Phrases simples correctes; contresens sur le vocabulaire moins courant (« manette » rendu par « coin », « je baisse les ombres » par « I kiss the shadows », « une petite pause » par « a small pot »). |
| `ggml-base` | 141 Mo | 0,50 à 0,62 s | Équivalente à la version quantifiée. |
| `ggml-small-q5_1` | 181 Mo | 2,1 à 2,3 s | Nettement meilleure : « controller », « I lower the shadows », « a short break »; une erreur sur « le chat » (« the cat »). |
| `ggml-small` | 465 Mo | 2,0 à 2,2 s | Équivalente à la version quantifiée. |

- Le temps dépend peu de la durée de la phrase : l'encodeur traite toujours une fenêtre de 30 secondes.
- La quantification q5_1 divise la taille par 2,5 sans effet mesurable sur le temps ni sur le texte.
- **Contexte audio réduit (`audio_ctx`) :** ajusté à la durée de la phrase, il ramène `base` à 0,17-0,45 s, mais de façon instable. Trop serré, il fait répéter ou inventer du texte et relance le décodage pendant plusieurs secondes (jusqu'à 6,4 s constatés); même avec une marge large (durée + 5 s, plancher 10 s), une phrase a pris 1,6 s sur une des trois exécutions. Le plugin utilise donc le contexte complet. `TRANSCRIBER_FIT_AUDIO_CONTEXT` reste disponible dans `src/transcriber.c` pour l'affichage progressif, à ne retenir qu'après de nouvelles mesures.
- **Essai à la voix dans OBS (2026-10-08) :** avec `base`, délai jugé bon (0,53 s en moyenne sur 18 énoncés d'après le journal) mais traductions trop souvent hors sujet; avec `small`, traductions jugées nettement meilleures et délai acceptable.
- **Choix actuel :** `ggml-small-q5_1`, téléchargé à la configuration CMake (SHA-256 vérifié) et installé avec le plugin, avec jusqu'à 8 threads (la moitié des cœurs physiques). Temps de `small` selon les threads, sur les mêmes phrases : 2,0 à 2,2 s avec 4, 1,5 à 1,7 s avec 6, 1,35 à 1,55 s avec 8, 1,1 à 1,4 s avec 12. À remesurer sur une machine plus modeste et pendant un jeu.

### Option B mesurée : transcription française puis traduction dédiée (2026-10-08)

Constat : `base` transcrit le français presque sans faute sur les neuf phrases, en 0,5 à 0,65 s; c'est sa traduction intégrée qui produit les contresens. Mesure de la traduction seule, hors plugin, avec la bibliothèque Python `ctranslate2` (int8, 4 threads, faisceau de 4) sur le texte français sorti de `base` :

| Modèle de traduction | Taille mesurée | Temps par phrase | Qualité observée |
|---|---|---|---|
| OPUS-MT fr-en (conversion `gaudi/opus-mt-fr-en-ctranslate2`) | 154 Mo | 0,03 à 0,25 s | Au niveau de `small` ou mieux : « controller », « I lower the shadows », « a little five minutes' break ». |
| OPUS-MT tc-big fr-en (conversion `craftwise/ct2-opus-mt-tc-big-fr-en-int8`) | 238 Mo | 0,06 à 0,48 s | Comparable, formulations un peu plus soignées. |

- Total estimé pour l'option B : 0,55 à 0,9 s par phrase, contre 1,35 à 2,2 s pour `small` seul, à qualité au moins égale sur cet échantillon.
- Limites de cette mesure : les erreurs de transcription se propagent (« est bienvenue » donne « is welcome »); la roue Python utilise Intel MKL, alors qu'un build embarqué dans le plugin utiliserait sans doute un autre backend, plus lent; les conversions testées viennent de tiers et ne serviraient pas telles quelles en distribution (conversion à refaire depuis le modèle Helsinki-NLP, Apache-2.0); l'audio est de la synthèse vocale.
- Coût d'intégration : CTranslate2 (MIT) et SentencePiece (Apache-2.0) à compiler et lier, un modèle de plus à livrer, une étape de traduction en C++. Non décidé.

## Tests et performance

Couvrir les composants au fur et à mesure de leur implémentation : buffer circulaire (écrasement, lecture, reset), VAD (silence/voix/transitions), transcription et traduction, moteur de sous-titres (provisoire/final, remplacement, expiration, découpage) et pipeline intégré. Réutiliser les tests et outils existants; ne pas créer une infrastructure de test spéculative.

Pour les essais audio manuels, varier microphones, silence/bruit, voix et durées. Surveiller stabilité, mémoire, CPU/GPU et latence. Ne pas qualifier le système de « temps réel » sans mesure reproductible.

Compiler et tester à chaque étape pertinente. Après une modification, privilégier d'abord une vérification ciblée; ne pas prétendre qu'un test a réussi s'il n'a pas été exécuté.

Pour l'intégration OBS, vérifier au minimum l'ajout, l'activation/désactivation, la suppression de la source, le déchargement du plugin et la fermeture d'OBS. Confirmer que la capture s'arrête lorsqu'elle est désactivée et qu'aucun worker/callback n'accède à des ressources détruites. Vérifier aussi que les scènes, profils et réglages OBS non détenus par LiveSubtitles restent inchangés. Utiliser la version OBS et les outils réellement documentés pour la cible; signaler les vérifications qui nécessitent un test manuel.

Pour l'installation Windows, prévoir une entrée de désinstallation. Le désinstalleur ne supprime que les fichiers appartenant à LiveSubtitles et ne modifie pas les scènes, profils ou réglages OBS. La suppression des modèles ou réglages propres à l'utilisateur doit être distincte et explicitement choisie. Si une installation manuelle est prise en charge, documenter comment retirer uniquement les fichiers du plugin. Tester l'installation puis la désinstallation avec OBS fermé et vérifier qu'aucun fichier tiers n'est supprimé.

## Documentation et collaboration

Garder le README compréhensible par des streamers non développeurs : fonctionnalités, prérequis, installation, utilisation, modèles, dépannage, confidentialité et licence. Documenter les décisions techniques importantes près de leur domaine.

Quand des commits sont demandés, utiliser des messages descriptifs et cohérents (par exemple `feat: add circular audio buffer`, `fix: handle microphone disconnect`, `test: cover subtitle expiration`). Ne pas créer de commit sans demande explicite.
