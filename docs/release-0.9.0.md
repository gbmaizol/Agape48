# Agape48 0.9.0

The first public release: an HP 48 on Windows, Linux and Android, from one source tree and one skin.

`0.9` rather than `1.0` on purpose. Everything in it works, and two things are missing: no ROM ships with it, and the Android build has never been measured for speed. Those two are what `1.0.0` waits for.

## Downloads

| | |
|---|---|
| Windows | `Agape48-0.9.0-windows-x64-setup.exe` |
| Linux | `Agape48-0.9.0-linux-x86_64.tar.gz` |
| Android | `Agape48-0.9.0-arm64-v8a.apk` |

Installing takes a minute per machine and is written out step by step, warnings included, in the [README](../README.md#installing-it). The one fiddly part is the ROM, which is a free download from hpcalc.org — also written out in full, [there](../README.md#then-give-it-a-rom-and-this-is-the-only-fiddly-step).

## If you already have the 0.1.0 APK, read this first

0.1.0 was never released publicly, so this affects only the handful of people who were given one directly. It is signed with a different key from 0.9.0, and **Android will not install one over the other.** What you will see is:

```
App not installed
```

and in a log, `INSTALL_FAILED_UPDATE_INCOMPATIBLE`. That is not a broken download and not a bug — it is Android refusing to replace an app with one signed by a different key, which is the protection that stops somebody else's build from impersonating this one.

**Copy your calculators off the phone before you uninstall.** Uninstalling deletes `Android/media/br.gbmaizol.agape48/`, and that is where they live. Then:

1. Copy `Android/media/br.gbmaizol.agape48/` somewhere safe — a computer, or anywhere outside that folder.
2. Uninstall Agape48.
3. Install 0.9.0.
4. Copy the folder back.

Upgrades from 0.9.0 onwards keep the same key and will install straight over the top, so this is a one-time cost, paid now precisely so that it never has to be paid by anybody who arrives later.

## Limits worth knowing before you start

- **No ROM is included.** It is HP's code, not ours. HP has allowed the ROM images to be downloaded since mid-2000 and they are on hpcalc.org; the README says exactly which file to take and what to do with it.
- **The Windows installer is not code-signed**, so Windows will show *"Windows protected your PC"* once. A signing certificate is a purchase, not a build flag.
- **The Android APK is signed with a self-signed key**, so Play Protect will warn about an unrecognised developer once.
- **Linux is x86_64 only**, built against glibc 2.39 — Ubuntu 24.04, Mint 22, Debian 13 or newer — and tested on X11. The Wayland plugin ships but is unmeasured.
- **Android is arm64-v8a only**, Android 8.0 or newer.
- **Android speed is not calibrated yet.** The real-speed switch is honest on the desktops and approximate on a phone.

## Licence

GPLv3. The full text is in [LICENSE](../LICENSE). Agape48 stands on the `x48` Saturn core by Eddie C. Dost and on the years of work Droid48 put into it, both under the GPL, so it could never have been anything else.
