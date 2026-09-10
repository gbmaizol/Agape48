#!/bin/sh
# Agape48 - install for one user, no root.
#
# Everything lands under ~/.local, which is the XDG place for a program a user
# installs for themselves: no sudo, no /opt, and uninstall.sh takes all of it
# back out again. The Windows half of this project is an Inno Setup installer
# into Program Files, which needs an administrator; this one deliberately does
# not, because nothing here has to be shared between the machine's users.
#
# The Qt runtime travels in lib/, plugins/ and qml/ next to the program, and
# bin/qt.conf is what points the program at them. That is why the whole tree
# moves as a unit and why bin/agape48 is not copied anywhere on its own.
#
# THIS FILE IS IN THE REPOSITORY, and that is the point of it. Until 2026sep09
# the installer was a script that existed only inside the built tarball, with
# the .desktop entry typed into a here-document and one 256-pixel icon beside
# it. Nothing regenerated either, so when the icon became a photograph of a real
# 48GX the installed one stayed the old drawn face - the same rot that had just
# been found in installer/agape48.ico on the Windows side. The entry and the
# icons now come out of the payload, where `cmake --install` put them from
# platform/linux/, and this script only decides WHERE.
set -e

here=$(cd "$(dirname "$0")" && pwd)
share="${XDG_DATA_HOME:-$HOME/.local/share}"
prefix="$share/agape48"
bindir="$HOME/.local/bin"
apps="$share/applications"
icons="$share/icons/hicolor"

echo "Installing Agape48 into $prefix"
rm -rf "$prefix"
mkdir -p "$prefix" "$bindir" "$apps"
cp -a "$here/bin" "$here/lib" "$here/plugins" "$here/qml" "$prefix/"
cp "$here/LICENSE" "$prefix/LICENSE"
cp "$here/uninstall.sh" "$prefix/uninstall.sh"
chmod +x "$prefix/uninstall.sh"

# Every size the payload carries, not just the big one: a menu draws 24 or 32
# and scaling 256 down at draw time is what makes an icon look muddy in a list.
for dir in "$here"/share/icons/hicolor/*/apps; do
    size=$(basename "$(dirname "$dir")")
    mkdir -p "$icons/$size/apps"
    cp "$dir/agape48.png" "$icons/$size/apps/agape48.png"
done

cat > "$bindir/agape48" <<EOF
#!/bin/sh
exec "$prefix/bin/agape48" "\$@"
EOF
chmod +x "$bindir/agape48"

# The one line the payload's entry cannot know: it ships Exec=agape48, which is
# right for an install into /usr or /usr/local, and wrong here because a tree
# under ~/.local/share is on nobody's PATH. Everything else - the name, the
# icon, the categories, StartupWMClass - comes from the file itself, so there is
# one source for the menu entry and it is the one in the repository.
sed "s|^Exec=.*|Exec=$prefix/bin/agape48|" \
    "$here/share/applications/agape48.desktop" > "$apps/agape48.desktop"

update-desktop-database "$apps" >/dev/null 2>&1 || true
gtk-update-icon-cache -f -t "$icons" >/dev/null 2>&1 || true

echo
echo "Done. Agape48 is in the menu under Education, and \"agape48\" runs it"
echo "from a terminal if $bindir is on your PATH."
echo "To remove it again: $prefix/uninstall.sh"
