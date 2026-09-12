# Ce fork

Ce dépôt est un fork de `KDE/systemsettings` — lui-même un miroir de
`invent.kde.org/plasma/systemsettings`. Tout ce qui vient de KDE est intact et
n'a pas été modifié : `app/`, `categories/`, `runner/`, `doc/`, `po/`.

Ce qui est à nous vit dans un seul dossier :

```
kcms/filesync/
```

C'est un module de synchronisation NAS bidirectionnelle et son greffon pour les
réglages système. Il ne dépend d'aucun fichier de ce dépôt et ne se compile pas
avec lui — c'est un projet autonome, rangé ici. Voir `kcms/filesync/README.md`.

Aucun fichier de l'amont n'étant touché, une resynchronisation depuis KDE ne
produira jamais de conflit.
