# Vendoring the x48 core

Last updated: 2026aug26-00h05

Agape48 does not fork x48's history. It vendors a chosen upstream under `src/core/x48/` and adapts it behind `x48_shim.h`. Nothing above `x48_shim.c` ever includes an x48 header.

## Which upstream

| Candidate | Why | Why not |
| --- | --- | --- |
| `x48ng` (gwenhael-le-moine) | Already split into core + swappable frontends (x11 / sdl / ncurses). `step_instruction()` is factored out, which is exactly the entry point `x48_run_slice()` needs. | **Archived 2026-07-29**, along with that author's whole calculator portfolio (`x48`, `x50ng`, `saturnng`, `hpemung`). Frozen, not abandoned mid-refactor, so still the best base - but nobody upstream will take patches. Slightly diverged from classic x48 behaviour in places. |
| `x48` (Eddie C. Dost, 0.6.4) | The reference. Every later fork descends from it. | `emulate()` is one unbounded loop welded to an X11 event pump. Splitting it is the biggest single piece of surgery in this project. |
| `Droid48`'s fork | Already solved the "no X11, JNI frontend, cycle-budgeted stepping" problem, and its LCD and keyboard glue is the closest to what Agape48 needs. | Android-shaped assumptions to unwind; check the licence and attribution before lifting code. |

Recommendation: start from `x48ng`, and read Droid48's frontend glue for how it budgets cycles and pumps the LCD.

## Steps

1. Add the sources: `git submodule add <url> src/core/x48`, then uncomment and correct the file list in `src/core/CMakeLists.txt`.
2. Define `AGAPE48_X48_VENDORED` for the `agape48_x48core` target.
3. Fill in the `TODO(vendor)` bodies in `x48_shim.c`, in this order: `x48_init` -> `x48_run_slice` -> `x48_take_frame` -> `x48_key_down/up`. After those four, the calculator is usable.
4. Delete or stub upstream's X11, ncurses, SDL and debugger frontends from the build. On a typical x48ng checkout that is the whole of `src/x11.c`, `src/sdl.c`, `src/ncurses.c` and `src/debugger.c` - together the largest single chunk of dead weight you can drop.
5. Stub `serial.c` unless you actually want HP 48 serial I/O; it pulls in termios and gains you nothing on Android.

## The ROM

x48 needs an HP 48 ROM image. Corrected 2026aug26 after reading Droid48: HP's ACO allowed non-commercial use of the HP 48 ROMs in autumn 2000, and Droid48 has bundled both the 512 KB 48G and the 256 KB 48S ROM in its Play Store APK for years, with an in-app note citing that permission. So Agape48 **can** ship a ROM, provided the release stays non-commercial.

That removes the first-run ROM import flow from the critical path. Two conditions before relying on it: read the actual wording of the ACO permission rather than Droid48's paraphrase of it, and keep the app free - "non-commercial" is the whole basis of the grant.

Keep the ROM path out of the state directory either way, so a cloud-synced folder does not end up carrying half a megabyte of ROM back and forth on every save.
