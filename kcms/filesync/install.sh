#!/bin/bash
# Installe le moteur, les unités et le module KDE.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"

install -Dm755 "$here/bin/filesync"       "$HOME/.local/bin/filesync"
install -Dm755 "$here/bin/filesync-watch" "$HOME/.local/bin/filesync-watch"
echo "moteur installé dans ~/.local/bin"

mkdir -p "$HOME/.config/systemd/user"
install -m644 "$here"/systemd/*.service "$here"/systemd/*.timer "$HOME/.config/systemd/user/"
systemctl --user daemon-reload
echo "unités installées — activer avec :"
echo "  systemctl --user enable --now filesync-watch.service filesync-periodic.timer"

if [ ! -f "$HOME/.config/filesync/filesync.conf" ]; then
  mkdir -p "$HOME/.config/filesync"
  sed "s|/home/you|$HOME|g" "$here/filesync.conf.example" > "$HOME/.config/filesync/filesync.conf"
  echo "config créée depuis l'exemple — vérifier les chemins distants"
fi

# Le module KDE demande une compilation et les droits root pour l'installer.
echo
echo "Pour le module des réglages système :"
echo "  cd $here/kcm && cmake -B build -DCMAKE_INSTALL_PREFIX=/usr && cmake --build build"
echo "  sudo cmake --install build"
