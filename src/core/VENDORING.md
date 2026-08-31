# Vendoring the x48 core

Last updated: 2026aug28-22h19

Agape48 does not fork x48's history. It vendors a chosen upstream under `src/core/x48/` and adapts it behind `x48_shim.h`. Nothing above `x48_shim.c` ever includes an x48 header.

## Which upstream

| Candidate | Why | Why not |
| --- | --- | --- |
| `x48ng` (gwenhael-le-moine) | Already split into core + swappable frontends (x11 / sdl / ncurses). `step_instruction()` is factored out, which is exactly the entry point `x48_run_slice()` needs. | **Archived 2026-07-29**, along with that author's whole calculator portfolio (`x48`, `x50ng`, `saturnng`, `hpemung`). Frozen, not abandoned mid-refactor, so still the best base - but nobody upstream will take patches. Slightly diverged from classic x48 behaviour in places. |
| `x48` (Eddie C. Dost, 0.6.4) | The reference. Every later fork descends from it. | `emulate()` is one unbounded loop welded to an X11 event pump. Splitting it is the biggest single piece of surgery in this project. |
| `Droid48`'s fork | Already solved the "no X11, JNI frontend, cycle-budgeted stepping" problem, and its LCD and keyboard glue is the closest to what Agape48 needs. | Android-shaped assumptions to unwind; check the licence and attribution before lifting code. |

**Decided by Gert on 2026aug28: start from Droid48's fork.** Its shape matches the Droid48-everywhere UI decision, it has already solved cycle-budgeted stepping without X11, and `x48ng` was archived on 2026-07-29, so neither candidate has a live upstream to send patches to.

Vendor `app/src/main/jni/` only. The Java layer under `app/src/main/java/org/ab/x48/` is replaced by the Qt/QML frontend, which keeps the licence position simpler - see item 7 of `docs/design-questions.md`.

## Steps

1. Add the sources: `git submodule add <url> src/core/x48`, then uncomment and correct the file list in `src/core/CMakeLists.txt`.
2. Define `AGAPE48_X48_VENDORED` for the `agape48_x48core` target.
3. Fill in the `TODO(vendor)` bodies in `x48_shim.c`, in this order: `x48_init` -> `x48_run_slice` -> `x48_take_frame` -> `x48_key_down/up`. After those four, the calculator is usable.
4. Delete or stub the remaining dead frontend weight. Droid48 already removed X11, SDL and ncurses, so what is left is `debugger.c` - keep only `rpl.c`'s `real_number()`, which item 3 of `docs/design-questions.md` still uses for the clipboard number path.
5. Stub `serial.c` unless you actually want HP 48 serial I/O; it pulls in termios and gains you nothing on Android.

## The ROM

x48 needs an HP 48 ROM image. Droid48 bundles both the 512 KB 48G and the 256 KB 48S ROM in its APK - verified 2026aug28 as `assets/rom` and `assets/roms` in the built package.

The ACO wording was read on 2026aug28 and does not support the confident version of this that stood here before. There is no primary text: hpcalc.org says HP "graciously began allowing this to be downloaded in mid-2000", Emu48's FAQ says ACO allowed the *use* of the ROMs without owning the calculator, and Emu48's manual declines to bundle them because "there's no license for the distribution of the ROM images". ACO itself was dissolved in November 2001. Item 7 of `docs/design-questions.md` has the full quotes.

So bundling is a risk call, not a permission - a well-precedented one, given Droid48's years on the Play Store, but a risk call.

**Decided by Gert on 2026aug28: bundle both, the 48GX and the 48SX**, taken knowingly on the grounds that the exposure for an open-source non-commercial release is negligible. That keeps the first-run ROM import flow off the critical path, and the SX ROM is needed anyway for the 48S setting kept by 2c. Cost is 533 KB compressed and 768 KB installed; see item 7 of `docs/design-questions.md`.

Keep the ROM path out of the state directory either way, so a cloud-synced folder does not end up carrying half a megabyte of ROM back and forth on every save.
