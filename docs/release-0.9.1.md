# Agape48 0.9.1

Everything [report 17](https://github.com/gbmaizol/Agape48/blob/main/README.md) turned up, fixed. It was the first time anybody installed Agape48 from nothing on all three machines and followed the README instead of a person, and what it found was not a broken calculator — it was a stale icon, a missing window, and a page that assumed things.

Same calculator, same ROM handling, same file formats. If 0.9.0 works for you there is nothing here you have to have; if you are installing for the first time, take this one.

## Downloads

| | |
|---|---|
| Windows | `Agape48-0.9.1-windows-x64-setup.exe` |
| Linux | `Agape48-0.9.1-linux-x86_64.tar.gz` |
| Android | `Agape48-0.9.1-arm64-v8a.apk` |

Android upgrades in place over 0.9.0 — same signing key, so your calculators stay where they are and you do not uninstall anything.

## The icon is a different picture

Not a different photograph: a different part of the same one, and much closer in. It is a frame I cut by hand out of the old icon — the violet shift key, the green shift key and `ON`, with the keyboard running diagonally across it — and the two shift keys are painted back to the colours a real 48GX has and the dusty blue light of the photograph took away.

It is a circle now on Windows and Linux. That is not decoration: the corners the circle removes are the black shadow under `CANCEL` and the dark table behind the calculator, which a square icon carried as a black band along its bottom edge. On Android it stays a square, because there the launcher holds the scissors and cuts its own shape.

**And on Windows it is now really the icon.** The one compiled into `agape48.exe` had been two art changes behind since September 8th, so the Start menu was right and a pinned taskbar button was wrong. `windres` never knew the `.ico` was an input — a resource script's `1 ICON "path"` is not an `#include`, so the dependency scanner reported none, and the icon only ever changed when something else forced the resource to recompile.

## About Agape48

New, in the menu behind the underlined `48GX`, in its own section above the two items that close the program. Version, build, Qt, a couple of paragraphs, and the licence.

The build line is the commit the binary was made from and the date of that commit, written in by the build itself. It is not the wall clock: a stamp that changed on every link would relink a link-time-optimised binary every time you touched a QML file, and the question a build stamp exists to answer — *does this one contain the fix* — is answered better by the commit anyway. A `+` after it means the tree had uncommitted changes.

## Finding a ROM

The ROM step is the only fiddly part of Agape48 and it is the part the README was worst at. Three things it now says that it did not:

- **Press Ctrl-F and search that page for `HP 48GX Revision`.** hpcalc.org's emulator page is mostly emulators; the eleven ROM images are a long way down it.
- **Unzip it before you go looking for the file.** No file dialog on any of the three systems can see inside a `.zip`, which is the likeliest reason for "I downloaded it and the program cannot find it".
- **Sort your Downloads folder by date and take the oldest thing in it.** These ROMs carry their original timestamps, so the file you just downloaded is the one that looks twenty-five years old.

And the red *no ROM* strip now says all of that itself, with the address in it — **clickable**, straight to your browser. It is the one screen somebody without a ROM is certain to see, and it used to know about the problem and say nothing about the cure.

The README also stopped skipping a step: Settings is behind the underlined `48GX` in the top right corner of the calculator, and there is no other way in.

## The phone stops explaining your keyboard to you

Hold a key down on a phone for two seconds and it used to grow a yellow box naming the keys on your keyboard that press it. There is no keyboard. The box now appears only when one is actually connected — measured through `Configuration.keyboard`, which is how Android itself decides — and it lets go when your finger does.

## What is still missing, same as 0.9.0

No ROM ships with it, and the Android build has never been measured for speed. Those two are what `1.0.0` waits for.

## Licence

GPLv3, in [LICENSE](https://github.com/gbmaizol/Agape48/blob/main/LICENSE). Agape48 stands on the `x48` Saturn core by Eddie C. Dost and on the years of work Droid48 put into it, both under the GPL.
