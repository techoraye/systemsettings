# À faire

## 1. Les boutons d'action ne répondent pas

Signalé : « Preview everything », « Sync now », « Open log » ne font rien, avec
une erreur affichée. L'import et l'export de preset non plus.

**Ce qui a déjà été éliminé** — ne pas recommencer :

| Hypothèse | Vérifié | Résultat |
|---|---|---|
| `.so` plus vieux que les sources | `stat` | non — le build est postérieur |
| `konsole` absent | `command -v` | présent, `/usr/bin/konsole` |
| `konsole --hold -e` invalide | lancé en vrai | fonctionne |
| Boutons grisés par `nasMounted` | `findmnt -no FSTYPE /mnt/nas` | renvoie « autofs\ncifs », donc vrai |
| `filesync` ne connaît pas `dry` | lu dans le script | `sync`, `dry`, `status` existent |
| Log absent | `ls ~/.cache/filesync` | `filesync.log` existe, 21 Mo |
| `currentFile` retiré de FileDialog | en-tête Qt 6.11 | existe toujours (déprécié) |
| Erreur QML au chargement | `kcmshell6 kcm_filesync` | aucune sortie |

**Ce qui manque pour aller plus loin : le texte exact de l'erreur.** Le QML
l'affiche dans la bannière `root.notice`. Sans lui on cherche à l'aveugle.

Prochaine étape concrète :

```bash
kcmshell6 kcm_filesync 2>&1 | tee /tmp/kcm.log
# cliquer sur un bouton qui échoue, puis lire /tmp/kcm.log
```

Pistes non encore testées : le `PATH` vu par `QProcess::startDetached` depuis un
KCM chargé par `systemsettings` (différent de celui d'un shell), et le
comportement de `startDetached` quand le module est déchargé juste après.

## 2. Rendre la synchronisation nettement plus rapide

Demandé, à traiter plus tard. Pistes :

- `unison` relit l'arborescence complète à chaque passage. Sur 105 000 fichiers
  en CIFS c'est le coût dominant. Regarder `-fastcheck true` (par défaut sur
  Unix, mais pas garanti à travers CIFS), et surtout limiter la profondeur des
  chemins surveillés.
- `CPUQuota=60%` et l'E/S `idle` sur les unités bride volontairement. C'est bon
  pour le premier balayage de 249 Go, discutable pour les passages suivants.
  Envisager un bridage différent entre le balayage périodique et la
  synchronisation déclenchée à la main.
- CIFS ne remonte aucun événement : le balayage périodique est le seul moyen
  d'attraper les modifications faites côté NAS. Réduire son intervalle coûte
  cher ; l'augmenter retarde la détection. Mesurer avant de changer.
