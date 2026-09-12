#!/usr/bin/env bash
# Keep an Android device's screen awake for a testing session, and put it back
# to sleep afterwards.
#
#   tools/android-awake.sh on      wake it and hold it awake
#   tools/android-awake.sh off     let go, and sleep the screen (which locks it)
#   tools/android-awake.sh status  say what the screen is doing right now
#
# Gert, 2026sep12, while the OnePlus Pad 3 was on the cable: "Please make a
# script to keep the screen of the Tablet awake while you're using it, and then
# lock the screen when you're done."
#
# WHY `svc power stayon` AND NOT screen_off_timeout. The timeout is a user
# setting with a value worth keeping; `stayon` is the developer-options switch
# "Stay awake", it is a hold Android keeps only while the device is on power -
# USB counts - and pulling the cable ends it by itself.
#
# IT IS STILL THE OWNER'S SWITCH, AND THAT WAS MEASURED RATHER THAN ASSUMED.
# The first run of this script on Gert's Pad 3 printed
# stay_on_while_plugged_in=15 BEFORE it did anything: he already had "Stay
# awake" on. An `off` that simply wrote 0 would have turned off a setting of his
# that this script never turned on. So `on` reads the value first, writes only
# if it has to, and leaves a note of what it found; `off` puts that value back.
# With no note - a reboot, a different machine, someone running `off` first -
# `off` sleeps the screen and does not touch the setting at all.
#
# SLEEPING IS LOCKING, as long as a lock is set: Android runs the lock on
# screen-off. KEYCODE_SLEEP rather than KEYCODE_POWER because POWER is a toggle
# and would wake a screen that was already off.
#
# It does not unlock anything. A device with a PIN comes back to its lock screen
# and that is the owner's business, not this script's.
set -eu

here=$(cd -- "$(dirname -- "$0")" && pwd)

# adb is not on PATH in a plain shell here; the SDK is the fallback.
if command -v adb >/dev/null 2>&1; then
    ADB=adb
elif [ -x "$HOME/Android/Sdk/platform-tools/adb" ]; then
    ADB="$HOME/Android/Sdk/platform-tools/adb"
else
    echo "adb not found - install platform-tools or put adb on PATH" >&2
    exit 1
fi

action=${1:-status}
serial=${2:-}
[ -n "$serial" ] && ADB="$ADB -s $serial"

# One device or none is the normal case; say so plainly when it is neither,
# because every command below would otherwise fail with adb's own wording.
count=$($ADB devices | awk 'NR>1 && $2=="device"' | wc -l)
if [ "$count" -eq 0 ]; then
    echo "no device: $($ADB devices | awk 'NR>1 && NF' | head -3)" >&2
    echo "if it says 'no permissions', this laptop has no udev rule for its vendor id." >&2
    exit 1
elif [ "$count" -gt 1 ] && [ -z "$serial" ]; then
    echo "more than one device - pass the serial as the second argument:" >&2
    $ADB devices | awk 'NR>1 && NF' >&2
    exit 1
fi

name=$($ADB shell getprop ro.product.model 2>/dev/null | tr -d '\r')

# Where the note lives. Session state, so TMPDIR, and keyed by serial so two
# devices on one laptop cannot overwrite each other's.
id=${serial:-$($ADB devices | awk 'NR>1 && $2=="device" {print $1; exit}')}
note="${TMPDIR:-/tmp}/agape48-awake-${id}"

# mWakefulness is Awake, Asleep, Dreaming or Dozing.
wakefulness() {
    $ADB shell dumpsys power 2>/dev/null \
        | grep -m1 -oE 'mWakefulness=[A-Za-z]+' | cut -d= -f2 | tr -d '\r'
}
# The hold itself, read back rather than remembered: 0 none, 1 ac, 2 usb,
# 4 wireless, and `stayon true` sets the union of them.
stayon() {
    $ADB shell settings get global stay_on_while_plugged_in 2>/dev/null | tr -d '\r'
}

case "$action" in
on)
    before=$(stayon)
    if [ "$before" = "0" ] || [ -z "$before" ]; then
        printf '%s' "${before:-0}" > "$note"
        $ADB shell svc power stayon true
        held="held by this script"
    else
        rm -f "$note"
        held="already held by the device's own \"Stay awake\""
    fi
    $ADB shell input keyevent KEYCODE_WAKEUP
    sleep 1
    echo "$name: awake, $held (stay_on_while_plugged_in=$(stayon)), screen $(wakefulness)"
    echo "run '$0 off' when done."
    ;;
off)
    if [ -f "$note" ]; then
        $ADB shell svc power stayon "$(cat "$note")"
        rm -f "$note"
        restored="hold released"
    else
        restored="hold left as it was - this script did not set it"
    fi
    sleep 1
    $ADB shell input keyevent KEYCODE_SLEEP
    sleep 1
    echo "$name: $restored (stay_on_while_plugged_in=$(stayon)), screen $(wakefulness)"
    ;;
status)
    echo "$name: screen $(wakefulness), stay_on_while_plugged_in=$(stayon)"
    ;;
*)
    sed -n '2,8p' "$here/$(basename "$0")" | sed 's/^# \{0,1\}//'
    exit 2
    ;;
esac
