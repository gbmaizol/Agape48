#!/bin/sh
# Takes back exactly what install.sh put down, and nothing else. Your
# calculators, your ROM and your settings are not here and are not touched:
# they live in ~/.local/share/Agape48 and ~/.config/Agape48, or in whatever
# synced folder you pointed the program at.
set -e
share="${XDG_DATA_HOME:-$HOME/.local/share}"
rm -rf "$share/agape48"
rm -f "$HOME/.local/bin/agape48"
rm -f "$share/applications/agape48.desktop"
# Every size, and only ours: the theme's other icons and the empty size folders
# belong to whoever else put something there.
for dir in "$share"/icons/hicolor/*/apps; do
    rm -f "$dir/agape48.png"
done
update-desktop-database "$share/applications" >/dev/null 2>&1 || true
gtk-update-icon-cache -f -t "$share/icons/hicolor" >/dev/null 2>&1 || true
echo "Agape48 removed. Your calculators and settings were left where they are."
