# Architecture cible

Ce document décrit des objectifs d'architecture, pas nécessairement des fonctions déjà implémentées. Vérifie toujours le code et les dépendances présentes avant de modifier ou d'annoncer un comportement.

## Pipeline

```text
Microphone -> capture audio -> buffer borné -> VAD -> reconnaissance vocale locale
            -> texte source -> traduction locale -> moteur de sous-titres -> source OBS
```

Séparer les responsabilités :

- La partie OBS crée/configure la source, affiche les erreurs et rend l'état courant des sous-titres.
- Le traitement audio/IA gère capture, buffer, VAD, transcription et traduction.
- Le moteur de sous-titres reste indépendant d'OBS. Il gère texte provisoire/final, expiration, découpage et remplacement des résultats provisoires.
- La transcription et la traduction restent deux rôles distincts, derrière des interfaces remplaçables. Pour le MVP français vers anglais, un même backend peut remplir les deux en une seule inférence (tâche de traduction de Whisper); l'interface doit permettre de revenir à deux étapes séparées (transcription puis traduction dédiée) sans toucher au reste du pipeline.

## Audio et temps réel

- Garder un buffer circulaire de capacité fixe, initialement de l'ordre de 5 à 10 secondes; écraser les données les plus anciennes plutôt que d'accumuler l'audio.
- Utiliser le VAD pour éviter les inférences coûteuses pendant le silence.
- Traiter progressivement avec des fenêtres glissantes ou une stratégie incrémentale; ne pas attendre systématiquement la fin d'un long buffer.
- Éviter les doublons de transcription, les traductions répétées et les inférences simultanées non maîtrisées.
- Ne jamais exécuter STT ou traduction sur le thread de rendu OBS. Synchroniser l'état partagé et éviter les data races.
- Garder bornées les files d'attente et l'historique audio, texte intermédiaire et logs.

## Modèles et intégration

- La reconnaissance et la traduction doivent fonctionner localement par défaut, sans API cloud obligatoire.
- Garder les backends derrière des interfaces pour pouvoir les remplacer.
- Un processus worker séparé est une option si cela améliore la stabilité ou isole les dépendances lourdes; ne l'ajoute pas sans besoin démontré.
- Automatiser la sélection raisonnable du matériel/modèle autant que possible; les réglages avancés ne sont pas destinés à l'interface principale.
- Vérifier les licences des modèles et dépendances avant toute intégration ou redistribution.

## Intégration OBS et limites d'interaction

- Vérifier la version OBS et le SDK réellement ciblés. Utiliser uniquement les API et callbacks documentés pour cette cible; ne pas supposer qu'une API ou qu'un chemin d'installation est compatible.
- S'intégrer comme source OBS ajoutée et configurée par l'utilisateur. Ne pas modifier les scènes, profils, réglages audio ou autres sources OBS.
- Rendu du texte (implémenté, `src/subtitle-source.c`) : la source `livesubtitles_source` possède une instance privée de la source texte intégrée d'OBS (`text_gdiplus`, dernière version via `obs_get_latest_input_type_id`) et la dessine dans son `video_render`. Cette instance est invisible pour l'utilisateur, créée et libérée avec la source. Ses réglages (`color`, `opacity`, `bk_color`, `bk_opacity`, `font`, `outline`) serviront aux options de style sans code de rendu propre. Dépendance : le module `obs-text` livré avec OBS sous Windows.
- Ne traiter que le microphone explicitement sélectionné, et seulement quand l'utilisateur a activé LiveSubtitles. À l'arrêt ou à la désactivation, arrêter la capture et le traitement. Ne pas capturer l'audio système ou d'autres sources en arrière-plan.
- Capture (implémentée, `src/subtitle-source.c`) : les réglages `audio_source` (UUID de la source audio OBS choisie) et `enabled` (faux par défaut) décrivent l'état voulu; `video_tick` y aligne l'état réel en ajoutant ou retirant le callback `obs_source_add_audio_capture_callback`. Le callback reçoit l'audio au format de sortie d'OBS, ignore l'audio d'une source mise en sourdine, le convertit en 16 kHz mono et l'écrit dans un tampon circulaire de 10 secondes (`src/audio-ring.c`). À la désactivation, le callback est retiré et le tampon vidé. Une référence forte sur la source captée est tenue pendant la capture et relâchée dès que l'utilisateur la supprime.
- Fil de traitement (implémenté) : chaque source LiveSubtitles possède un worker, créé avec elle et joint à sa destruction. Lui seul démarre et arrête la capture, charge le modèle et exécute la détection de parole; `video_tick` se limite à lire l'état sous verrou pour l'afficher. Le worker consomme le tampon par fenêtres de 512 échantillons, les passe à Silero VAD (`src/voice-activity.c`) puis à un détecteur à hystérésis (`src/speech-detector.c`) qui produit des états parole/silence stables. Les modèles ne sont chargés qu'après activation.
- Transcription (implémentée) : pendant la parole, le worker accumule l'audio dans un tampon d'énoncé borné à 20 secondes, précédé des 0,3 seconde qui ont devancé la détection. À la fin de la parole, ou quand le tampon est plein, il le passe à `src/transcriber.c` (Whisper, dans la langue parlée), traduit le texte avec `src/translator.cpp` (OPUS-MT via CTranslate2, un modèle `opus-mt-<parlée>-<sous-titres>` par paire de langues; aucune traduction quand les deux langues sont les mêmes) et publie la traduction sous verrou; `video_tick` l'affiche. Une seule inférence à la fois : pendant qu'elle tourne, l'audio continue d'arriver dans le tampon circulaire. Affichage progressif : pendant la parole, dès 3 secondes d'énoncé puis à chaque fois que 2 secondes s'y sont ajoutées, et seulement quand tout l'audio en attente est traité, le worker reconnaît et traduit tout l'énoncé depuis son début et publie ce texte provisoire, que la passe suivante ou la passe finale remplace; aucune déduplication n'est donc nécessaire. Moteur de sous-titres (implémenté, `src/subtitle-engine.c`, indépendant d'OBS) : le worker publie chaque traduction avec un compteur de version; `video_tick` la remet au moteur, qui la découpe en lignes dont la longueur dépend de la taille du texte (48 caractères à la taille par défaut, pour qu'une ligne pleine remplisse presque la zone), n'en garde que les 2 dernières et la fait disparaître en fondu de 0,6 seconde après 60 ms d'affichage par caractère (entre 3 et 7 secondes). Le fondu passe par les réglages `opacity` et `bk_opacity` de la source texte d'OBS, en 10 paliers. Style (implémenté) : taille du texte, couleur du texte, couleur et opacité du fond se règlent dans les propriétés et sont transmis à cette source texte privée, sans zone fixe pour que le fond suive le texte; `video_render` la centre en bas d'une zone fixe de 1600 x 300 et la réduit si elle dépasse. Entre deux sous-titres l'image reste vide; la ligne d'état avec le niveau sonore ne s'affiche qu'avant le premier sous-titre d'une capture. Le journal ne reçoit que des durées, jamais le texte.
- API OBS appelées depuis le worker : `obs_get_source_by_uuid`, `obs_source_add_audio_capture_callback`, `obs_source_remove_audio_capture_callback`, `obs_source_removed`, `obs_source_release`, `obs_get_audio_info`. La documentation d'OBS ne se prononce pas sur leur usage hors du thread principal; leur sûreté a été vérifiée dans les sources de libobs 31.1.1 (listes protégées par mutex, compteurs de références atomiques, destruction des sources différée sur un thread dédié).
- Respecter le cycle de vie documenté de la source et du plugin. À la destruction ou à la fermeture, arrêter et joindre les workers, détacher les callbacks et libérer les ressources appartenant au plugin; aucune tâche ne doit continuer à accéder à l'état détruit.
- Ne jamais bloquer le thread de rendu OBS avec capture, STT, traduction ou attente de worker. Garder les callbacks audio/rendu courts; n'appeler les API OBS depuis un worker que si la documentation de la version cible l'autorise ou, à défaut, après vérification dans les sources de libobs de cette version, en consignant ici les fonctions concernées.
- Garder l'interface simple et ne pas lancer de capture, téléchargement de modèle ou autre activité coûteuse avant une action explicite de l'utilisateur, sauf comportement clairement annoncé et nécessaire au fonctionnement.
