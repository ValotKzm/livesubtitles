# LiveSubtitles : consignes du projet

## Objectif et priorités

LiveSubtitles est un plugin OBS de sous-titrage traduit en temps réel. La cible initiale est Windows 10/11 et le premier couple de langues est français vers anglais; à terme, la langue parlée et la langue des sous-titres doivent pouvoir être choisies librement. Le produit vise les streamers non techniques, avec un fonctionnement gratuit, local par défaut et sans compte, clé API ou serveur obligatoire.

Priorités, dans l'ordre : simplicité, stabilité, latence, qualité, fonctionnalités. Favorise toujours la solution qui permet à un streamer de commencer à utiliser le logiciel en quelques minutes. Reporte les options avancées et le périmètre non essentiel.

## Règles de travail

- Comprends le code, les dépendances et les tests près de la zone avant de modifier. Le code vérifié prime sur une architecture cible ou une idée future décrite dans la documentation.
- Fais le changement minimal qui répond à la tâche. Ne réécris pas des composants sans nécessité.
- Ne suppose pas qu'une API, une version OBS, une bibliothèque, un script ou une commande existe : vérifie-le dans le dépôt, les en-têtes ou la documentation officielle.
- Après chaque changement pertinent, exécute d'abord la vérification ciblée disponible, puis élargis aux tests ou à la compilation si le risque le justifie. Rapporte exactement ce qui a été vérifié.
- N'ajoute pas de dépendance ou d'abstraction sans besoin démontré. Vérifie licences, compatibilité et impact de distribution avant intégration.
- Ne crée pas de commit sans demande explicite.
- Travaille sur une branche par étape (`feat/...`, `fix/...`, `docs/...`), fusionnée dans `main` avec un commit de merge une fois l'étape vérifiée, puis supprimée en local et sur le dépôt distant.
- Les messages de commit et de merge ne mentionnent aucun outil d'IA et ne portent aucune ligne de co-auteur automatique.
- L'utilisateur vérifie lui-même le comportement dans OBS : installe le plugin, décris l'essai à faire, puis lis le journal d'OBS une fois OBS fermé.

## Confidentialité et stabilité

- L'audio reste local par défaut. Ne transmets pas et n'enregistre pas silencieusement l'audio ou la conversation.
- Garde l'audio, les files de traitement et les historiques en mémoire bornés; ne journalise pas les phrases prononcées par défaut.
- Ne bloque jamais le thread de rendu OBS avec le traitement audio ou IA. Synchronise correctement tout état partagé.
- Présente des erreurs compréhensibles à l'utilisateur et garde les détails techniques dans les logs sans données conversationnelles inutiles.

## Documentation à consulter selon la tâche

- Architecture du pipeline, audio, VAD, STT, traduction, sous-titres ou OBS : `docs/architecture.md`.
- MVP, interface, confidentialité, critères d'acceptation ou évolution produit : `docs/product.md`.
- Dépendances, licences, décisions techniques, tests, compilation, sécurité ou distribution : `docs/development.md`.

Ces documents décrivent des exigences ou des cibles, pas nécessairement des fonctionnalités déjà présentes. Consulte seulement ceux qui sont pertinents pour la tâche. Mets à jour la documentation concernée lorsqu'un comportement cible ou une décision importante change.
