# filesync

Synchronisation de dossiers locaux vers un NAS — à double sens ou à sens
unique, avec ou sans propagation des suppressions — pilotée depuis un module
natif des réglages système KDE.

```
Réglages système  →  File Sync
```

## Ce que c'est

Trois morceaux qui vont ensemble :

- **`bin/filesync`** — le moteur. Lit `~/.config/filesync/filesync.conf` et
  synchronise chaque paire de dossiers déclarée.
- **`bin/filesync-watch`** — le surveillant. Déclenche une synchronisation quand
  un dossier local se calme.
- **`kcm/`** — le module KDE. Ajoute une page « File Sync » aux réglages système
  pour gérer les paires sans éditer de fichier.

Plus les unités systemd utilisateur qui font tourner tout ça.

## Pourquoi unison et pas rsync

rsync est unidirectionnel. Le lancer dans les deux sens ne permet pas de
distinguer « supprimé ici » de « créé là-bas » : les suppressions ressuscitent
au passage suivant, et un vrai conflit n'est jamais détecté.

unison garde une archive par paire de l'état accordé la dernière fois. C'est
exactement ce qui rend la synchronisation à double sens sûre, et c'est la seule
raison de la dépendance.

## Ce que CIFS impose

Le NAS est monté en CIFS, ce qui contraint plusieurs choix :

- **CIFS ne remonte aucun événement** pour les modifications faites sur le NAS
  lui-même. `inotify` ne voit que le côté local — d'où le balayage périodique
  complet, qui est le seul moyen d'attraper l'autre direction.
- **CIFS ne peut pas restituer les modes Unix** : la propriété est fixée par
  `uid=`/`gid=` au montage. Les permissions ne sont donc pas synchronisées, elles
  le seraient de travers.
- Le premier balayage transfère beaucoup (environ 249 Go dans le cas d'origine)
  et dure des heures, d'où `TimeoutStartSec=infinity` sur l'unité périodique : le
  délai par défaut d'un `oneshot` la tuerait en route.

## Pourquoi c'est bridé

Les deux unités tournent en `Nice=19`, classe d'E/S `idle`, `CPUWeight=20` et
`CPUQuota=60%`. C'est du travail de fond : il ne doit jamais être la raison pour
laquelle les ventilateurs se mettent en route. Le quota empêche un balayage de
105 000 fichiers sur CIFS d'occuper un cœur pendant une heure.

## Installation

```bash
# le moteur et le surveillant
install -Dm755 bin/filesync        ~/.local/bin/filesync
install -Dm755 bin/filesync-watch  ~/.local/bin/filesync-watch

# les unités
install -Dm644 systemd/*.service systemd/*.timer -t ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now filesync-watch.service filesync-periodic.timer

# le module KDE
cd kcm && cmake -B build -DCMAKE_INSTALL_PREFIX=/usr && cmake --build build
sudo cmake --install build
```

Dépendances : `unison`, `inotify-tools`, et pour le module
`extra-cmake-modules`, Qt6 (Core, Quick), KF6 (KCMUtils, CoreAddons, I18n).

Puis copier `filesync.conf.example` vers `~/.config/filesync/filesync.conf` et
adapter les chemins — ou tout faire depuis Réglages système → File Sync.

## Configuration

```ini
[General]
enabled=true
debounce=5              # secondes de calme avant de déclencher
periodicMinutes=15      # balayage complet, pour attraper les changements côté NAS
conflictPolicy=newer
notify=true

[Pair_Documents]
local=/home/you/Documents
remote=/mnt/nas/NAS/Backup/Documents
direction=push          # twoway | push (local -> NAS) | pull (NAS -> local)
deletions=keep          # keep | mirror
enabled=true
```

Une section `[Pair_*]` par paire de dossiers.

## Sens et suppressions : deux réglages, pas un

Ce sont deux questions indépendantes, et les confondre coûte des données.

`direction` dit **d'où** part l'autorité. `deletions` dit ce qu'il advient de ce
que la destination possède **en propre** :

| | `deletions=keep` (défaut) | `deletions=mirror` |
|---|---|---|
| `push` | le NAS reçoit et garde tout | le NAS devient identique au dossier local |
| `pull` | le local reçoit et garde tout | le local devient identique au NAS |
| `twoway` | aucune suppression ne se propage | une suppression d'un côté se propage |

Le piège est qu'un sens unique **n'est pas additif par nature**. Dans unison,
`force = racine` résout *toute* différence en faveur de cette racine — et
« ce fichier n'existe que de l'autre côté » en est une. Sans `deletions=keep`,
un `push` efface donc du NAS tout ce qui n'est pas dans le dossier local. Vider
le dossier local puis lancer une synchro vidait l'archive.

`deletions=keep` ajoute `nodeletion = <destination>`, qui interdit toute
suppression dans cette racine quoi qu'il arrive par ailleurs.

Deux garde-fous complètent le dispositif :

- une racine **absente** (montage tombé, dossier renommé) n'est pas une racine
  vide : la paire est refusée au lieu d'être traitée comme « tout a disparu » ;
- en mode miroir, une source **vide** face à une destination **pleine** est
  refusée. C'est la signature d'un accident, jamais d'une intention.

## Détails qui ont coûté du temps

- **Le montage est en autofs.** `findmnt -no FSTYPE /mnt/nas` renvoie le
  déclencheur *et* le montage réel, donc le test porte sur « contient cifs » et
  non sur une égalité.
- **`kcmutils_add_qml_kcm` refuse de tourner** sans
  `CMAKE_LIBRARY_OUTPUT_DIRECTORY` et consorts, que `KDECMakeSettings` ne définit
  pas pour une compilation hors de l'arbre source KDE.
- **`QT_MAJOR_VERSION` doit précéder `KDEInstallDirs`**, sinon ECM retombe sur la
  disposition Qt5 et cherche un qmake qui n'existe pas.

## Licence

Usage personnel.
