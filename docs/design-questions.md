# Holes in the plan, and what to do about them

Last updated: 2026aug26-00h05

Ordered by how much trouble each one causes if it is discovered late. Every item has options rather than a verdict, except where one option is clearly better.

## 1. Splitting x48's main loop is the actual project

`x48_run_slice(cycles)` assumes something that does not exist upstream. Classic `x48`'s `emulate()` is an unbounded loop that owns the X11 event pump, the timers and the LCD refresh all at once. Everything else in Agape48 is a few days of work; this is the part that can eat a month.

- **Start from `x48ng`.** `step_instruction()` is already factored out. Cheapest path to a running calculator.
- **Start from Droid48's fork.** Droid48 already solved exactly this problem - cycle-budgeted stepping, no X11, a JNI frontend - and its LCD and keyboard glue is the closest existing code to what Agape48 needs. Check its licence and attribution before lifting any of it.
- **Start from classic x48 and do the surgery.** Only if you want the reference behaviour bit-for-bit and are willing to pay for it.

Recommendation: read Droid48's frontend glue, then build on `x48ng`.

## 2. Stopping the tick will drift the clock

`Agape48Engine::tick()` stops the timer when `x48_is_asleep()` is true. On a phone that is not optional - a 60 Hz timer that never sleeps is a battery complaint. But the HP 48 has real timers: the system clock, alarms, and the timer2 interrupt. If emulation stops for four hours of wall clock and then resumes, the calculator believes four hours did not happen.

- Advance the Saturn timer registers by elapsed wall clock on wake. Correct, and it is what the HP 48 hardware effectively does; needs care in the shim.
- Keep ticking at a very low rate (1 Hz) instead of stopping. Simpler, still cheap, keeps the clock roughly honest.
- Ignore it. Alarms silently do not fire, which is a real Droid48-parity regression.

Recommendation: 1 Hz idle tick now, wall-clock catch-up later if alarms matter.

## 3. Clipboard to and from the RPL stack is not a small feature

x48 has no object decompiler. Getting stack level 1 out as text means one of:

- **Drive the calculator.** Push the object, run `→STR` by synthesising keystrokes, read the resulting string object out of RAM. ~50 lines, always agrees with what the HP 48 itself would print. Needs the calculator to be at a clean stack prompt - not in the EquationWriter, not in a menu, not mid-edit - so it needs a guard and a graceful refusal.
- **Port an RPL object walker.** Faster and state-independent, but it is a decompiler for every object type and you now own it.

Recommendation: drive the calculator, refuse politely when the machine is not at a clean prompt. Same in reverse for paste.

## 4. Android SAF file descriptors are the wrong shape for a 1990s C core

`StateFileManager::populateAndroidSaf()` hands the core four detached file descriptors. That works only if the vendored x48's `romio.c` is rewritten from `fopen(path)` to `fdopen(fd)`, and it assumes the fds stay valid for the whole session - they do not survive the user revoking the grant, and a document provider that restarts can invalidate them.

- **Copy in, copy out.** On start, read the state out of the SAF folder into app-private storage; on save/suspend, write it back. The core only ever sees ordinary POSIX paths, no upstream surgery at all, and the copy-back is the natural place to detect that the remote file changed underneath you.
- **fdopen surgery.** Zero copying, more invasive, more failure modes.

Recommendation: copy in, copy out. This is better than what is currently in `StateFileManager.cpp` and the fd path should probably be deleted.

## 5. KML support may not be worth shipping in the binary

Emu48 skins are `.bmp` faces drawn for a 1990s desktop at LCD zoom 1 or 2. They load fine - `QImage` reads BMP with no extra module - but at 200-400 px wide they are unusable on a phone. So runtime KML support buys a large library of skins that mostly look wrong on the platform that matters most.

- **Runtime KML parser**, as written. Good for desktop, costs binary size and adds a text parser to the crash surface.
- **`tools/kml2json.py`, a build-time converter.** Skin authors convert once; the app ships one format. Removes `KmlParser.*` from the binary, which is the size goal, and the conversion is where upscaling and re-tracing has to happen anyway.
- **Both**, with the parser behind a CMake option that is off for mobile.

Recommendation: the converter. Keep `KmlParser.*` in the tree as the converter's engine, not as app code.

## 6. Neither the key table nor the scancode table is filled in

`kKeys` in `Agape48Engine.cpp` and `kScanMap` in `KmlParser.cpp` both carry all the structure and none of the numbers. That is deliberate - inventing 49 matrix codes produces a keypad that looks perfect and types the wrong characters, which is a far worse bug than a loud failure. Both tables get copied out of the vendored fork's keyboard table in one sitting. Until then `pressKey` reports the unmapped key by name.

## 7. Licensing has to be settled before the first binary, not after

- x48 is GPL, so Agape48 is GPL. The source ships with the binaries either way.
- Statically linking Qt under the LGPL obliges you to let a recipient relink against a modified Qt. In practice: publish the object files, or publish the complete build recipe.
- Check whether x48 is GPLv2 or v3. Droid48 answers half of it: its `COPYING` is GPLv3, and so is czodroid's. GPLv3 plus app-store distribution terms has caused friction before; v2 has not. x48 itself still needs checking.
- ~~The HP 48 ROM cannot ship in the APK.~~ Wrong, corrected 2026aug26. HP's ACO allowed non-commercial use of the HP 48 ROMs in autumn 2000, and Droid48 has shipped both the 48G and 48S ROMs on the Play Store for years on that basis. Agape48 can bundle one as long as the release is free. Read the actual permission wording before relying on it.
- A photorealistic face of an HP calculator is trade dress, and "HP 48" in a store listing is a trademark use. Droid48 has lived with this for years, so the risk is evidently low, but it is not zero. Keeping the app name Agape48 and describing it as a Saturn/RPL calculator emulator costs nothing.

## 8. Two brief items contradict themselves

- "Use basic QSoundEffect" and "do not include Multimedia" cannot both hold: `QSoundEffect` *is* Qt Multimedia. Resolved in README under "Sound and haptics" - a small per-platform `Feedback` singleton, no Qt Multimedia.
- "60 fps LCD" and "smallest possible footprint / phone battery" pull against each other. Resolved by drawing only on change: `x48_take_frame()` returns false when the HP 48 drew nothing, which is most frames.

## 9. Smaller things worth knowing

- Emulating on the GUI thread is a deliberate choice (a 4 MHz interpreter is a rounding error on a modern core). The cost is that a long QML relayout or a JS garbage collection stalls emulation and jitters the beep. Acceptable for a calculator; revisit only if measured.
- `LcdItem` currently allocates a fresh `QSGTexture` on every changed frame. Reusing one `QSGPlainTexture` and calling `setImage()` avoids the churn. Small, worth doing.
- `MultiPointTouchArea` with `mouseEnabled` gives desktop exactly one touch point, so `ON + A + F` on desktop only works through the physical-keyboard path. That makes the `Qt.Key` mapping table load-bearing rather than a convenience.
- `-Os` on a Saturn interpreter dispatch loop can cost real throughput. `AGAPE48_CORE_OPTIMIZE_FOR_SPEED` exists for that, and it is worth one measurement rather than an assumption - on a phone, `-Os` may genuinely win on battery even if it loses on cycles.
- Droid48 also does serial file transfer (Kermit/XModem) for moving objects on and off the calculator. Not in the brief. If "Droid48 parity" is meant literally, it belongs on the list; if not, `serial.c` gets stubbed out and saves a few KB.
- One portrait face at 480 px wide will look soft on a modern phone. A 1080-wide WebP at quality 80 is roughly 250-400 KB, which is affordable. A separate landscape face doubles that; programmatic rotation of the portrait face is free but looks wrong. Worth deciding before the photograph is taken.
