# Instructions Claude Code

@AGENTS.md

## Charger le contexte utile

En début de session, lis `NOTES.md` : il dit où le travail s'est arrêté, la prochaine tâche et les pièges déjà rencontrés. Mets-le à jour en fin de session.

Ne lis pas toute la documentation à chaque tâche. Avant une modification, consulte le document correspondant :

- Pipeline, audio, VAD, STT, traduction, sous-titres ou intégration OBS : `docs/architecture.md`.
- Périmètre MVP, expérience utilisateur, confidentialité ou critères d'acceptation : `docs/product.md`.
- Dépendances, décisions techniques, compilation, tests, sécurité ou distribution : `docs/development.md`.
- Choix de la prochaine étape, jalons du projet ou révision de la feuille de route : `PLAN.md`.

Ne consulte pas `PLAN.md` pour une correction ou une tâche locale sans impact sur les jalons. Pour une tâche de planification, compare son contenu à l'état réel du dépôt, ne marque un jalon terminé qu'après vérification et ne lance pas tout le plan d'un seul coup.

Si une tâche touche plusieurs domaines, consulte seulement les documents concernés. Le code existant et les contraintes vérifiées priment sur les architectures encore envisagées dans la documentation. Signale et corrige la documentation concernée lorsqu'une décision ou un comportement cible change.
