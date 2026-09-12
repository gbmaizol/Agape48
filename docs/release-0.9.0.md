# Agape48 0.9.0

The first release: an HP 48 on Windows, Linux and Android, from one source tree and one skin.

`0.9` rather than `1.0` on purpose. Everything in it works, and two things are missing: no ROM ships with it, and the Android build has never been measured for speed. Those two are what `1.0.0` waits for.

## Downloads

| | |
|---|---|
| Windows | `Agape48-0.9.0-windows-x64-setup.exe` |
| Linux | `Agape48-0.9.0-linux-x86_64.tar.gz` |
| Android | `Agape48-0.9.0-arm64-v8a.apk` |

Installing takes a minute per machine, and is written out step by step — including the two warnings your system will show you — in the [README](https://github.com/gbmaizol/Agape48/blob/main/README.md#installing-it). The one fiddly part is the ROM, which is a free download from hpcalc.org, and that is written out in full [there](https://github.com/gbmaizol/Agape48/blob/main/README.md#then-give-it-a-rom-and-this-is-the-only-fiddly-step).

## What it will and will not run on

- **Windows** 10 or newer, 64-bit.
- **Linux**, x86_64, built against glibc 2.39 — Ubuntu 24.04, Mint 22, Debian 13 or newer. Tested on X11.
- **Android** 8.0 or newer, arm64-v8a, which is every phone sold since about 2016.

Two things you will be told once and can wave through: the Windows installer is not code-signed, so Windows says *"Windows protected your PC"*; and the Android package is signed with its own key rather than through the Play Store, so Play Protect mentions an unrecognised developer. Both are about where the file came from, not what is in it, and the README says which button to press.

**No ROM is included** — it is HP's code, not ours. HP has allowed the HP 48 ROM images to be downloaded since mid-2000 and they sit on hpcalc.org; the README names the exact file to take and what to do with it.

## Licence

GPLv3. The full text is in [LICENSE](https://github.com/gbmaizol/Agape48/blob/main/LICENSE). Agape48 stands on the `x48` Saturn core by Eddie C. Dost and on the years of work Droid48 put into it, both under the GPL, so it could never have been anything else.
