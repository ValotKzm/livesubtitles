# Feuille de route LiveSubtitles

Ce fichier suit les jalons du projet. Ce n'est ni une spécification détaillée ni une preuve que les fonctions décrites existent. Vérifier le dépôt avant de changer les statuts. Ne déclarer un jalon terminé qu'après avoir vérifié son critère de sortie.

## Cible

MVP Windows 10/11 pour OBS Studio : sous-titres français vers anglais, traitement local par défaut, sans compte ni clé API, utilisable par un streamer non technique.

Les objectifs produit sont dans [docs/product.md](docs/product.md), l'architecture cible dans [docs/architecture.md](docs/architecture.md), et les règles de validation/dépendances dans [docs/development.md](docs/development.md).

## État courant

Au 2026-10-08, le dépôt contient le squelette issu du modèle officiel `obs-plugintemplate` : un module OBS vide qui journalise son chargement. Il compile avec les commandes de [docs/development.md](docs/development.md) et se charge puis se décharge proprement dans OBS 32.2.2 (vérifié dans le journal d'OBS). Aucun commit, aucun test, aucune brique audio ou IA.

La source `livesubtitles_source` affiche un texte de test statique. Vérifié manuellement dans OBS 32.2.2 : ajout, affichage, suppression, déchargement du plugin et fermeture d'OBS sans erreur du plugin ni fuite mémoire (`Number of memory leaks: 0`). Depuis, la source a des propriétés (microphone, activation), capte l'audio de la source choisie dans un tampon borné et affiche son état à la place du texte de test; cette partie compile et ses tests unitaires passent, mais n'est pas encore vérifiée dans OBS.

**Prochaine étape : vérifier la capture dans OBS, puis seconde partie du jalon 3 (whisper.cpp et VAD).**

## Jalons

### 0. État des lieux et décisions techniques - Terminé (2026-10-08)

Inspecter le dépôt et l'environnement cible. Vérifier la version/SDK OBS à cibler et comparer les choix nécessaires pour le langage et le build, la capture audio, le VAD, le STT local, la traduction locale et la distribution Windows. Vérifier compatibilité, maintenance et licences. Consigner les décisions et compromis dans la documentation de développement.

**Sortie attendue :** décisions justifiées par des sources vérifiées, périmètre du premier incrément et commande de build identifiée. Ne pas commencer par développer tout le pipeline.

### 1. Squelette OBS compilable - Terminé (2026-10-08)

Créer le plus petit plugin compatible avec les choix validés et vérifier qu'il compile sur l'environnement cible.

**Sortie attendue :** build reproductible; chargement ou procédure de vérification OBS documentée.

### 2. Source OBS minimale - Terminé (2026-10-08)

Enregistrer une source LiveSubtitles avec les API documentées de la version OBS vérifiée et afficher un texte de test statique. Vérifier son ajout, sa configuration, sa suppression et le cycle de vie du plugin, sans modifier les scènes/profils ni bloquer le thread OBS.

**Sortie attendue :** source ajoutable dans OBS et texte visible; aucun changement collatéral dans OBS; vérifications ciblées réussies.

### 3. Audio borné et détection de parole - En cours

Première partie (réglages, capture, tampon borné) : écrite, tests du tampon réussis, vérification manuelle dans OBS à faire. Seconde partie (whisper.cpp et VAD) : à faire.

Ajouter les contrôles minimaux de sélection du microphone et d'activation, puis la capture du microphone choisi uniquement après activation, un buffer de capacité fixe et le VAD. À la désactivation, arrêter la capture et le traitement. Ne pas capturer l'audio système ni d'autres sources en arrière-plan. Garder les files et l'état partagés bornés et synchronisés.

**Sortie attendue :** tests de buffer et de transitions silence/parole; vérifier manuellement que seul le micro choisi est traité après activation et que le traitement s'arrête à la désactivation; aucune capture avant activation ni accumulation avec la durée du stream.

### 4. Transcription locale progressive - À faire

Intégrer le backend STT choisi derrière une interface. Produire des résultats progressifs si le backend le permet et éviter les doublons entre fenêtres.

**Sortie attendue :** tests sur audio de référence et mesure initiale de la latence de transcription.

### 5. Traduction et sous-titres - À faire

Intégrer la traduction locale français vers anglais et un moteur de sous-titres indépendant d'OBS. Gérer le remplacement provisoire/final, l'expiration et le rendu du résultat dans la source.

**Sortie attendue :** parcours audio vers sous-titre vérifié de bout en bout; tests ciblés du moteur de sous-titres.

### 6. Configuration et modèles - À faire

Finaliser la configuration nécessaire au MVP : langues, persistance des réglages, présentation de l'état et réglages de lisibilité (couleur du texte, fond de couleur et d'opacité réglables, voir [docs/product.md](docs/product.md)). Automatiser l'installation ou la gestion des modèles si nécessaire; présenter des erreurs compréhensibles.

**Sortie attendue :** configuration conservée correctement et démarrage sans manipulation technique non prévue pour l'utilisateur.

### 7. Stabilisation et distribution MVP - À faire

Compléter les tests pertinents, vérifier confidentialité, mémoire bornée, nettoyage à l'arrêt et comportement sur une session prolongée. Préparer packaging/installateur et guide utilisateur selon les choix validés. Inclure une désinstallation qui retire uniquement les fichiers LiveSubtitles; ne supprimer modèles et réglages propres à l'utilisateur que sur demande explicite.

**Sortie attendue :** scénario MVP de [docs/product.md](docs/product.md) passé sur l'environnement cible; test d'installation/désinstallation sans altérer les données OBS ou fichiers tiers; limites et vérifications documentées.

## Après le MVP - non planifié

À réévaluer après stabilisation du MVP : options de style avancées pour les sous-titres, choix automatique des modèles selon le matériel, langues supplémentaires et éventuel portage macOS/Linux. Ces pistes ne doivent pas retarder le MVP.

## Décisions encore ouvertes

Tranché au jalon 0 (voir [docs/development.md](docs/development.md)) : version cible d'OBS, langage et système de build, capture audio, VAD, STT, licence du plugin, traduction en une étape pour démarrer.

Encore ouvert : confirmation par mesure de la traduction en une étape (jalon 5), taille de modèle par défaut, mode de livraison des modèles (installateur ou téléchargement annoncé), packaging.

## Règle de progression

Valider chaque jalon avant de dépendre de lui. Lancer les tests et compilations ciblés pendant le développement, pas uniquement à la fin. Si le code ou une contrainte vérifiée invalide l'ordre proposé, mettre à jour cette feuille de route et expliquer le changement plutôt que de suivre une étape devenue inadaptée.
