# Produit et périmètre

## Objectif

LiveSubtitles est un plugin OBS destiné à afficher en temps réel des sous-titres traduits de la parole d'un streamer. Cible initiale : Windows 10/11 et OBS Studio; premier couple de langues : français vers anglais.

Priorité produit : simplicité, stabilité, latence, qualité, puis fonctionnalités supplémentaires. Évalue chaque fonctionnalité selon cette question : un streamer non technique peut-il l'installer et l'utiliser en quelques minutes ? Sinon, simplifie, automatise ou reporte.

Le projet vise une utilisation gratuite, locale/offline par défaut, sans compte, clé API ni serveur obligatoire.

## Expérience MVP

L'interface principale doit rester simple : choisir le microphone, la langue parlée et la langue des sous-titres, activer le traitement et voir son état. Masquer les réglages techniques (GPU, threads, quantification, seuils VAD, chemins des modèles, etc.) hors d'une éventuelle section avancée.

Critère d'acceptation principal : l'utilisateur installe le plugin, l'ajoute comme source OBS, choisit le microphone et Français -> English, active le traitement, prononce « Bonjour tout le monde et bienvenue sur mon stream » et voit apparaître une traduction proche de « Hello everyone and welcome to my stream. » Le flux normal ne doit nécessiter ni compte, ni clé API, ni serveur distant obligatoire, ni configuration technique manuelle.

Le comportement doit rester sous le contrôle de l'utilisateur : le traitement du microphone commence uniquement après activation explicite et s'arrête à la désactivation. LiveSubtitles n'écoute que le microphone choisi; il ne capture pas l'audio système ou d'autres sources OBS en arrière-plan et ne modifie pas les scènes, profils ou réglages OBS. L'état actif/inactif doit être compréhensible dans l'interface : une ligne « État » dans les propriétés de la source l'indique (implémenté le 2026-10-09). L'image, vue par les viewers, reste vide quand tout va bien ou quand la source est désactivée; elle n'affiche un message que si le streamer doit agir (microphone non choisi ou indisponible, fichier de modèle introuvable).

## Confidentialité et erreurs

- L'audio reste local par défaut. Ne pas transmettre ni enregistrer silencieusement l'audio ou la conversation.
- Ne pas conserver plusieurs heures d'audio; ne pas journaliser les phrases prononcées par défaut.
- Toute collecte distante éventuelle doit être explicite, documentée et contrôlée.
- Présenter des erreurs compréhensibles pour l'utilisateur; conserver les détails techniques dans les logs sans données conversationnelles inutiles.

## Lisibilité des sous-titres

Implémenté le 2026-10-09 (demande du 2026-10-08, complétée par la taille du texte) : permettre de choisir la couleur du texte et d'afficher un fond derrière les sous-titres, de couleur et d'opacité réglables, pour garantir la lisibilité sur n'importe quelle image. La taille du texte est réglable, et une ligne pleine occupe presque toute la largeur de la zone de la source, quelle que soit cette taille. Ces réglages restent peu nombreux et visibles dans l'interface principale; les valeurs par défaut doivent déjà être lisibles sans y toucher.

Demande du 2026-10-08, après essai à la voix : afficher le texte au fur et à mesure pendant une longue prise de parole plutôt que d'un bloc à la fin, puis l'effacer progressivement pour ne pas surcharger l'image. L'affichage progressif est l'objet de la seconde étape du jalon 4; l'effacement progressif relève du moteur de sous-titres (jalon 5).

## Langues : objectif précisé le 2026-10-08

Le but est de toucher le plus de streamers possible : la langue parlée et la langue des sous-titres doivent pouvoir être choisies librement, dans les deux sens, et pas seulement vers l'anglais (par exemple anglais vers français ou russe, français vers une autre langue). Langues visées à terme : français, anglais, russe, espagnol, puis d'autres. Pour l'instant, se limiter au français et à l'anglais, dans les deux sens : implémenté le 2026-10-09, avec deux listes dans les propriétés de la source. Choisir la même langue des deux côtés donne des sous-titres non traduits.

Conséquence : la traduction intégrée de Whisper, qui n'écrit qu'en anglais, ne suffit pas; la traduction doit être une étape distincte de la transcription. Le streamer ne doit pas avoir à gérer des modèles : il choisit deux langues, le plugin se charge du reste.

## Évolution envisagée

Ces éléments sont des pistes, pas des exigences à implémenter d'avance : améliorer latence et gestion des modèles, étendre les langues (espagnol, allemand, italien, portugais), puis évaluer macOS/Linux. Garder le MVP petit et valider chaque étape avant d'élargir le périmètre.
