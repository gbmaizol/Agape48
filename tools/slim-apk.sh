#!/usr/bin/env bash
# Take the built APK down to what this app can actually reach, then align and
# sign it.
#
#   tools/slim-apk.sh <unsigned.apk> <out.apk> <keystore> <alias> <storepass>
#
# WHY THIS IS A POST-STEP AND NOT THE BUILD. androiddeployqt bundles what
# qmlimportscanner finds, and QtQuick.Controls declares every style it has,
# because the style is normally a runtime choice. Ours is not: main.cpp pins it
# with QQuickStyle::setStyle("Basic"), per the Quick Controls decision of
# 2026aug28. So five of the six styles are shipped and can never load. The
# supported knob for this is QT_ANDROID_DEPLOYMENT_DEPENDENCIES, which is a
# WHITELIST of every library the app needs - get one wrong and it fails at
# runtime, on the device, far from the mistake. A blacklist of things provably
# unreachable is the safer shape while the list is short.
#
# WHAT IS SAFE TO DROP, AND HOW THAT WAS ESTABLISHED. Each style library is
# linked by exactly one thing: its own QML style plugin, which the engine loads
# only when that style's module is imported. Checked with objdump across all 72
# bundled libraries - nothing else names them. The image formats go because no
# asset in this app is anything but PNG, and PNG is inside Qt Gui rather than a
# plugin. libQt6Svg follows its plugin out.
#
# WHAT IS DELIBERATELY LEFT. libQt6QuickDialogs2QuickImpl is 3.4 MB and Android
# uses the SYSTEM file picker, so it looks like the biggest prize here - but the
# QtQuick.Dialogs QML module may import it at load time whether or not it draws
# anything, and getting that wrong breaks Import file to stack, which is the
# feature this round exists to test. It stays until it can be tested on a
# device. Same for QuickShapes, QuickEffects and Network - Network is not even
# optional, libQt6Qml links it.
set -euo pipefail

IN="${1:?usage: slim-apk.sh <in.apk> <out.apk> <keystore> <alias> <storepass>}"
OUT="${2:?}"; KS="${3:?}"; ALIAS="${4:?}"; PASS="${5:?}"

BT="$(ls -d "$HOME"/Android/Sdk/build-tools/* | sort -V | tail -1)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cp "$IN" "$WORK/app.apk"

STYLES="Material Fusion Universal Imagine FluentWinUI3"
FORMATS="qtiff qjpeg qwebp qsvg qtga qwbmp qicns"

DROP=()
for s in $STYLES; do
    low="$(echo "$s" | tr '[:upper:]' '[:lower:]')"
    DROP+=("lib/arm64-v8a/libQt6QuickControls2${s}_arm64-v8a.so")
    DROP+=("lib/arm64-v8a/libQt6QuickControls2${s}StyleImpl_arm64-v8a.so")
    DROP+=("lib/arm64-v8a/libqml_QtQuick_Controls_${s}_qtquickcontrols2${low}styleplugin_arm64-v8a.so")
    DROP+=("lib/arm64-v8a/libqml_QtQuick_Controls_${s}_impl_qtquickcontrols2${low}styleimplplugin_arm64-v8a.so")
done
for f in $FORMATS; do
    DROP+=("lib/arm64-v8a/libplugins_imageformats_${f}_arm64-v8a.so")
done
DROP+=("lib/arm64-v8a/libQt6Svg_arm64-v8a.so")

BEFORE=$(stat -c%s "$WORK/app.apk")
# -d rather than a rebuild: every entry that stays keeps the storage it had,
# which for a .so means stored-not-deflated and page-aligned, the way Android
# needs it to mmap them.
zip -q -d "$WORK/app.apk" "${DROP[@]}" || true
AFTER=$(stat -c%s "$WORK/app.apk")

"$BT/zipalign" -p -f 4 "$WORK/app.apk" "$WORK/aligned.apk"
"$BT/apksigner" sign --ks "$KS" --ks-key-alias "$ALIAS" --ks-pass "pass:$PASS" \
    --out "$OUT" "$WORK/aligned.apk"
"$BT/apksigner" verify "$OUT" >/dev/null && echo "signature verifies"

printf 'dropped %s entries: %d -> %d bytes (%d MB saved)\n' \
    "${#DROP[@]}" "$BEFORE" "$AFTER" "$(( (BEFORE - AFTER) / 1024 / 1024 ))"
printf '%s  %s\n' "$(sha256sum "$OUT" | cut -d' ' -f1)" "$(basename "$OUT")"
