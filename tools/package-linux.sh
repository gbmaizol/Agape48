#!/bin/sh
# Build the Linux tarball: the program, the Qt runtime it needs, the menu entry
# and the icons, plus install.sh and uninstall.sh.
#
#     tools/package-linux.sh [output directory]
#
# The output directory defaults to dist-linux/ BESIDE the checkout, which is
# where the APKs and the Windows installer already land - deliverables live next
# to the source tree, not inside it.
#
# Until 2026sep09 these steps were prose in the handover and the two scripts
# existed only in the built tarball. That is why the tarball's icon was four
# days out of date: nothing regenerated it and nothing could notice. Everything
# it copies is now a tracked file that `cmake --install` puts in place.
#
# THE PRUNE IS THE ONLY CLEVER PART, and it is measured, not guessed:
# qt_deploy_runtime_dependencies ships 125 MB, this cuts it to 97. What comes
# out is Qt's translations, the five Quick Controls styles we do not use (the
# style is pinned to Basic in main.cpp), and four plugin folders the app has no
# way to reach - qmltooling is the QML debugger, tls and networkinformation
# belong to Qt Network which is not linked at all, iconengines is SVG icons,
# egldeviceintegrations is for a machine with no window system. 38 MB of what
# is LEFT is ICU, which a prebuilt Qt Core is linked against and which nothing
# short of a static feature-trimmed Qt can remove - see README, "Size budget".
#
# The translations are copied and then deleted rather than never copied:
# qt_generate_deploy_qml_app_script takes NO_TRANSLATIONS, and it is left off on
# purpose so that a plain `cmake --install` still behaves exactly like stock Qt
# for anyone who is not building this tarball.
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
build=${BUILD:-$root/build}
out=${1:-$root/../dist-linux}

# The version, from the one place that defines it: project(... VERSION x.y.z).
# head -1 because qt_add_qml_module has a VERSION of its own, further down.
version=$(sed -n 's/^[[:space:]]*VERSION \([0-9][0-9.]*\)[[:space:]]*$/\1/p' \
    "$root/CMakeLists.txt" | head -1)
[ -n "$version" ] || { echo "cannot read VERSION from CMakeLists.txt" >&2; exit 1; }

name="Agape48-$version-linux-$(uname -m)"
stage="$out/staging"

[ -x "$build/agape48" ] || { echo "no build at $build - cmake --build it first" >&2; exit 1; }

# The one thing this script cannot fix by itself: the icons are generated and
# committed, so art that changed without a run of make-icon.py would be packaged
# stale. Warn rather than run it - Pillow is not a build dependency.
if [ "$root/assets/icon-source.png" -nt \
     "$root/platform/linux/hicolor/256x256/apps/agape48.png" ]; then
    echo "WARNING: assets/icon-source.png is newer than the icons." >&2
    echo "         Run: python3 tools/make-icon.py   (and commit what it writes)" >&2
fi

echo "Staging $name"
rm -rf "$stage"
mkdir -p "$out"
cmake --install "$build" --prefix "$stage" >/dev/null

rm -rf "$stage/translations"
for style in FluentWinUI3 Fusion Imagine Material Universal; do
    rm -f "$stage/lib/libQt6QuickControls2$style.so".* \
          "$stage/lib/libQt6QuickControls2${style}StyleImpl.so".*
    rm -rf "$stage/qml/QtQuick/Controls/$style"
done
for plugin in qmltooling tls networkinformation iconengines egldeviceintegrations; do
    rm -rf "$stage/plugins/$plugin"
done

install -m 755 "$root/platform/linux/install.sh" "$stage/install.sh"
install -m 755 "$root/platform/linux/uninstall.sh" "$stage/uninstall.sh"

rm -rf "$out/$name" "$out/$name.tar.gz"
cp -a "$stage" "$out/$name"
tar -C "$out" -czf "$out/$name.tar.gz" "$name"

echo
echo "$out/$name.tar.gz"
ls -l "$out/$name.tar.gz" | awk '{print "  " $5 " bytes"}'
du -sh "$out/$name" | awk '{print "  " $1 " unpacked"}'
echo "  install with: tar xzf $name.tar.gz && $name/install.sh"
