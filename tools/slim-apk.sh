#!/usr/bin/env bash
# Take the built APK down to what this app can actually reach, then align and
# sign it.
#
#   tools/slim-apk.sh <android-build-dir> <out.apk> <keystore> <alias> <storepass>
#
# where <android-build-dir> is the folder androiddeployqt staged, normally
# build-android/android-build. THE STAGING FOLDER, NOT AN APK, and that is the
# whole difference between this version and the one that crashed.
#
# ---------------------------------------------------------------------------
# WHAT WENT WRONG THE FIRST TIME, because it is worth writing down.
#
# Version one took the finished APK and ran `zip -d` on it, deleting eleven Qt
# libraries that nothing else LINKS - verified with objdump across all 72
# bundled libraries, and that verification was correct. Gert installed it:
# "The 48859115 had an error upon the first run and crashed. I had to install
# the 65807583 one."
#
# Native linkage was the wrong question. Qt's Java bootstrap does not wait to
# be asked for a library by the dynamic linker; QtLoader reads the string array
# `qt_libs` out of res/values/libs.xml and calls System.loadLibrary on every
# name in it, in order, before a line of C++ runs. Eleven of the names it reads
# were of files that were no longer in the APK, so the first one hit
# UnsatisfiedLinkError and took the process down at startup - which is exactly
# what "an error upon the first run" looks like from outside.
#
# An APK is therefore not a bag of files that can be pruned from the outside.
# It has a manifest of its own, in a binary resource table, and the two have to
# agree. So this script now works one step earlier, on the staging folder where
# libs.xml is still plain XML and libs/arm64-v8a still plain files, and lets
# gradle build a consistent APK from the smaller inputs.
#
# ---------------------------------------------------------------------------
# WHY THERE IS ANYTHING TO CUT. androiddeployqt bundles what qmlimportscanner
# finds, and QtQuick.Controls declares every style it has, because the style is
# normally a runtime choice. Ours is not: main.cpp pins it with
# QQuickStyle::setStyle("Basic"), per the Quick Controls decision of 2026aug28.
# So five of the six styles are shipped and can never load. The supported knob
# for this is QT_ANDROID_DEPLOYMENT_DEPENDENCIES, which is a WHITELIST of every
# library, plugin and QML module the app needs - get one wrong and it fails at
# runtime, on the device, far from the mistake. A blacklist of things provably
# unreachable is the safer shape while the list is short.
#
# WHAT IS SAFE TO DROP, AND HOW THAT WAS ESTABLISHED. Each style library is
# linked by exactly one thing: its own QML style plugin, which the engine loads
# only when that style's module is imported. The image formats go because no
# asset in this app is anything but PNG, and PNG is inside Qt Gui rather than a
# plugin. libQt6Svg follows its plugin out - the only two things that name it
# are libplugins_imageformats_qsvg and itself. All of that is re-checked below
# on every run rather than trusted from a comment.
#
# WHAT IS DELIBERATELY LEFT. libQt6QuickDialogs2QuickImpl is 3.4 MB and Android
# uses the SYSTEM file picker, so it looks like the biggest prize here - but the
# QtQuick.Dialogs QML module may import it at load time whether or not it draws
# anything, and getting that wrong breaks Import file to stack. It stays until
# it can be tested on a device. Same for QuickShapes, QuickEffects and Network -
# Network is not even optional, libQt6Qml links it.
set -euo pipefail

STAGE="${1:?usage: slim-apk.sh <android-build-dir> <out.apk> <keystore> <alias> <storepass>}"
OUT="${2:?}"; KS="${3:?}"; ALIAS="${4:?}"; PASS="${5:?}"

LIBS="$STAGE/libs/arm64-v8a"
LIBSXML="$STAGE/res/values/libs.xml"
[ -d "$LIBS"    ] || { echo "no $LIBS - is that the androiddeployqt staging dir?" >&2; exit 1; }
[ -f "$LIBSXML" ] || { echo "no $LIBSXML" >&2; exit 1; }

BT="$(ls -d "$HOME"/Android/Sdk/build-tools/* | sort -V | tail -1)"
NDK="$(ls -d "$HOME"/Android/Sdk/ndk/* | sort -V | tail -1)"
READELF="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf"

STYLES="Material Fusion Universal Imagine FluentWinUI3"
FORMATS="qtiff qjpeg qwebp qsvg qtga qwbmp qicns"

DROP=()
for s in $STYLES; do
    low="$(echo "$s" | tr '[:upper:]' '[:lower:]')"
    DROP+=("libQt6QuickControls2${s}_arm64-v8a.so")
    DROP+=("libQt6QuickControls2${s}StyleImpl_arm64-v8a.so")
    DROP+=("libqml_QtQuick_Controls_${s}_qtquickcontrols2${low}styleplugin_arm64-v8a.so")
    DROP+=("libqml_QtQuick_Controls_${s}_impl_qtquickcontrols2${low}styleimplplugin_arm64-v8a.so")
done
for f in $FORMATS; do
    DROP+=("libplugins_imageformats_${f}_arm64-v8a.so")
done
DROP+=("libQt6Svg_arm64-v8a.so")

# --- check 1: nothing that STAYS links anything that GOES --------------------
# The check that was right the first time, kept because it is still necessary -
# it just was not sufficient.
echo "checking native links of $(ls "$LIBS"/*.so | wc -l) libraries..."
bad=0
for f in "$LIBS"/*.so; do
    base="$(basename "$f")"
    case " ${DROP[*]} " in *" $base "*) continue;; esac
    for n in $("$READELF" -d "$f" 2>/dev/null | grep -oE 'lib[A-Za-z0-9_]+\.so' | sort -u); do
        case " ${DROP[*]} " in
            *" $n "*) echo "  REFUSING: $base still links $n" >&2; bad=1;;
        esac
    done
done
[ $bad -eq 0 ] || { echo "drop list is not safe; nothing changed" >&2; exit 1; }

# --- the cut, in both places at once -----------------------------------------
gone=0
for d in "${DROP[@]}"; do
    [ -f "$LIBS/$d" ] || continue
    rm -f "$LIBS/$d"
    gone=$((gone + 1))
    # libs.xml names libraries without the "lib" prefix or the ".so" - the
    # form QtLoader hands to System.loadLibrary. Anything not listed there
    # (the QML plugins, the image formats) simply has no line to remove.
    stem="${d#lib}"; stem="${stem%.so}"
    sed -i "\%<item>arm64-v8a;${stem}</item>%d" "$LIBSXML"
done
echo "removed $gone libraries and their libs.xml entries"

# --- check 2: THE ONE THAT WOULD HAVE CAUGHT THE CRASH ------------------------
# Every name QtLoader will load must be a file that exists. This is cheap, and
# it is the entire failure of version one.
echo "checking every qt_libs entry resolves to a file..."
missing=0
while read -r stem; do
    [ -n "$stem" ] || continue
    # qt_libs entries are the form System.loadLibrary takes - no "lib", no
    # ".so" - which is why only THAT array is read. load_local_libs, three
    # lines below it in the same file, holds whole filenames instead.
    [ -f "$LIBS/lib${stem}.so" ] || { echo "  MISSING: lib${stem}.so is in qt_libs" >&2; missing=1; }
done < <(sed -n '/<array name="qt_libs">/,/<\/array>/{s%.*<item>arm64-v8a;\(.*\)</item>.*%\1%p}' "$LIBSXML")
[ $missing -eq 0 ] || { echo "libs.xml names a library that is not there" >&2; exit 1; }

# --- repack ------------------------------------------------------------------
( cd "$STAGE" && ./gradlew --quiet assembleRelease )
BUILT="$STAGE/build/outputs/apk/release/android-build-release-unsigned.apk"
[ -f "$BUILT" ] || { echo "gradle produced no $BUILT" >&2; exit 1; }

"$BT/zipalign" -p -f 4 "$BUILT" "$BUILT.aligned"
rm -f "$OUT"
"$BT/apksigner" sign --ks "$KS" --ks-key-alias "$ALIAS" --ks-pass "pass:$PASS" \
    --out "$OUT" "$BUILT.aligned"
rm -f "$BUILT.aligned"
"$BT/apksigner" verify "$OUT" >/dev/null && echo "signature verifies"

# --- check 3: the same question again, of the APK that will be installed ------
# Belt and braces, and it costs a second. Reads the real resource table rather
# than guessing at file names, because a release APK renames its resources.
echo "checking the built APK the same way..."
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
"$BT/aapt2" dump resources "$OUT" > "$tmp/res.txt" 2>/dev/null
unzip -Z1 "$OUT" 'lib/arm64-v8a/*' > "$tmp/have.txt"
sed -n '/array\/qt_libs/,/^  type /p' "$tmp/res.txt" \
    | grep -oE '"arm64-v8a;[^"]+"' | tr -d '"' | sed 's/^arm64-v8a;//' > "$tmp/want.txt"
missing=0
while read -r stem; do
    grep -qx "lib/arm64-v8a/lib${stem}.so" "$tmp/have.txt" \
        || { echo "  MISSING IN APK: lib${stem}.so" >&2; missing=1; }
done < "$tmp/want.txt"
[ $missing -eq 0 ] || { echo "the APK would crash on startup; not shipping it" >&2; exit 1; }
printf 'all %d qt_libs entries present in the APK\n' "$(wc -l < "$tmp/want.txt")"

printf '%s  %s  (%d bytes)\n' "$(sha256sum "$OUT" | cut -d' ' -f1)" \
    "$(basename "$OUT")" "$(stat -c%s "$OUT")"
