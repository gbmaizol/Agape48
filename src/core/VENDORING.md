# Vendoring the x48 core

Last updated: 2026aug25-17h36

Agape48 does not fork x48's history. It vendors a chosen upstream under `src/core/x48/` and adapts it behind `x48_shim.h`. Nothing above `x48_shim.c` ever includes an x48 header.

## Which upstream

| Candidate | Why | Why not |
| --- | --- | --- |
| `x48ng` (gwenhael-le-moine) | Already split into core + swappable frontends (x11 / sdl / ncurses). `step_instruction()` is factored out, which is exactly the entry point `x48_run_slice()` needs. Actively maintained. | Slightly diverged from classic x48 behaviour in places. |
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

x48 needs an HP 48 ROM image, which is copyrighted and must not be committed or shipped inside the APK. Agape48 asks the user to supply one on first run. Keep that path out of the state directory so a cloud-synced folder does not end up carrying the ROM around.
