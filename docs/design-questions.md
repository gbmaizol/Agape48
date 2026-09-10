# Holes in the plan, and what to do about them

Last updated: 2026aug29-15h50

Ordered by how much trouble each one causes if it is discovered late. Every item has options rather than a verdict, except where one option is clearly better.

## 1. DECISION: start from Droid48's fork

`x48_run_slice(cycles)` assumes something that does not exist upstream. Classic `x48`'s `emulate()` is an unbounded loop that owns the X11 event pump, the timers and the LCD refresh all at once. Everything else in Agape48 is a few days of work; this is the part that can eat a month.

- **Start from `x48ng`.** `step_instruction()` is already factored out. Cheapest path to a running calculator.
- **Start from Droid48's fork.** Droid48 already solved exactly this problem - cycle-budgeted stepping, no X11, a JNI frontend - and its LCD and keyboard glue is the closest existing code to what Agape48 needs. Check its licence and attribution before lifting any of it.
- **Start from classic x48 and do the surgery.** Only if you want the reference behaviour bit-for-bit and are willing to pay for it.

**Decided by Gert on 2026aug28: start from Droid48's fork.** The earlier recommendation here was `x48ng`, and it predated 2c. Once the UI decision is Droid48 everywhere, the emulator and the frontend agree in shape, and `x48ng` was archived on 2026-07-29 in any case, so nobody upstream would take patches either way.

What gets vendored is `app/src/main/jni/` only - the C core, 45 files. The Java layer is discarded and replaced by the Qt/QML frontend, which matters for item 7. Done on 2026aug28 into `src/core/x48/`, as a plain copy rather than a submodule, mtimes preserved.

### DONE 2026aug28: the core boots on desktop through the shim

Vendored, compiled, linked and smoke-tested against the 48GX ROM. It reaches the HP 48 "Memory Clear" screen, reports `display.on`, contrast 14, and parks in SHUTDN. Verified with direct `gcc`, not yet through the project's CMake, which needs Qt present.

**The premise of this section was wrong, and cheaply so.** Splitting the main loop was never the expensive part: `step_instruction()` is already factored out in Droid48's fork at `emulate.c:1974`, and `emulate()` reduces to step, throttle, schedule. The bounded replacement is about twenty lines, appended to `emulate.c` as `agape48_emulate_begin()` and `agape48_emulate_slice()`. `emulate()` itself is untouched and simply goes uncalled. The throttle busy-wait - roughly 2 microseconds per instruction - is dropped, because Agape48 paces from a Qt timer instead.

**The real obstacle was somewhere else entirely.** `do_shutdown()` in `actions.c` parks the calculator in `do { blockConditionVariable(); ... } while (wake == 0 && exit_state);`. Under Droid48 that blocks a dedicated emulator thread on a pthread condition variable until Java fires an alarm. On the Qt GUI thread it would freeze the UI for as long as the calculator sleeps, which is most of the time.

It cost zero edits to `actions.c`. The escape is already in the while condition: clearing `exit_state` makes `do_shutdown` run one wake-check pass and return. And because `main.c` is not compiled, `blockConditionVariable()` is an unresolved symbol the shim is free to define. So the shim defines it as "do not block, let one pass happen, hand the thread back", the frontend drops to the 1 Hz idle tick from item 2, and each tick runs one more pass. The two decisions fit together without either being made for the other.

### What the integration actually cost

Six small edits to the vendored tree, all portability rather than behaviour:

- `x48.h` - guard `<android/log.h>` and `<jni.h>` behind `__ANDROID__`, with no-op `LOG*` macros elsewhere; guard the three JNI-typed globals the same way; add the include guard the header never had.
- `romio.c`, `errors.c` - drop their direct `<android/log.h>` includes.
- `lcd.c` - add `<stdint.h>`, and guard Droid48's three Java blit entry points.
- `device.c` - guard `fillAudioData`, the one JNI function left in a compiled file.

Everything else the seam absorbs. Droid48 replaced Xlib with dummy structs but left the call sites, so `disp`, `dpy`, `XClearWindow`, `XClearArea`, `XCopyPlane`, `XCreateBitmapFromData`, `adjust_contrast` and `ShowConnections` are all defined as no-ops in `x48_shim.c` rather than edited out of the files most likely to be re-vendored. `saturn` itself, `progname`, `exit_state`, `enter_debugger`, `in_debugger` and `disassembler_mode` live there too, for the same reason: they were `main.c`'s and `debugger.c`'s.

Built: `emulate actions memory register timer device init romio lcd errors rpl binio serial resources options`. Not built: `main.c` (JNI + `main()`), `x48.c` (X11 frontend), `debugger.c`, `disasm.c`.

### Two traps found while wiring it, both worth knowing

- **`init_emulator()` returns 0 on success**, and has no failure return at all - `read_rom()` calls `exit(1)` on a bad ROM. Testing it as a boolean inverts the check, which is how the first attempt failed. That `exit(1)` must become an error return before this is a GUI app: a bad ROM must not take the process down.
- **Paths are built by bare concatenation**: `strcpy(path, files_path); strcat(path, rom_filename);` with no separator inserted (`init.c:1159`, `1614`, `1804`). So `files_path` carries the trailing slash and the rest are bare leaf names. Getting it wrong produces `/state/dir/state/dir/rom` and a fatal exit with no message.

### A third trap, found on 2026aug28 while finishing item 1: nothing could wake the calculator

The first shim defined `GetEvent()` as a constant `0`, on the reasoning that `x48_key_down()` writes `saturn.keybuf` and calls `do_kbd_int()` directly, so there is no event queue left to pump. That is true while the CPU is running and false the moment it sleeps, which is most of the time.

`do_shutdown()` decides whether to wake from exactly three sources: the two Saturn timers, the serial port, and `GetEvent()`. Its keyboard branch reads

```c
interrupt_called = 0;
if (GetEvent()) {
    if (interrupt_called) wake = 1;
}
```

so the interrupt has to be raised *inside* that call. Raising it earlier from `x48_key_down` is invisible to the sleep loop: `interrupt_called` is zeroed on the line before. With `GetEvent()` returning a constant 0, a sleeping calculator could never be woken by a key - ON included - and Agape48 would have been a calculator you could switch on once.

The fix keeps a single delivery point. `x48_key_down()` sets the row bits and a pending flag; `GetEvent()` drains the flag and calls `do_kbd_int()`. Both paths pump it: `do_shutdown`'s wake loop, and `agape48_emulate_slice()` once per slice. That second pump is needed because the original pumped from `schedule()` under `got_alarm` (`emulate.c:2421`), and the SIGALRM that raised `got_alarm` lived in the frontend that is not compiled.

Raising the interrupt in both places instead would push the return address twice. The gate on which presses raise it copies `x48.c:381`: ON always raises, an ordinary key only on a fresh press and only while `saturn.kbd_ien` is set.

### Next, in order

1. ~~`init.c` must accept an absolute `rom_filename`.~~ **Done 2026aug28.** `agape48_rom_path()` in `init.c` takes an absolute `rom_filename` verbatim and otherwise keeps the old `files_path` + leaf concatenation; `write_files()` no longer writes a converted copy back over an absolute ROM. `x48_init()` honours `cfg->rom_path` and probes the file first. Verified three ways: the legacy layout still boots, a bundled ROM at an absolute path outside the state directory boots to Memory Clear, and a `rom_path` pointing at nothing returns false with a message instead of exiting. No ROM copy appears in the state directory afterwards, which is the 512 KB per install this was for.
2. ~~Turn `read_rom`'s `exit(1)` into an error return.~~ **Done 2026aug28.** See below - it turned into "the core must not call `exit()` at all", which is four sites, and a loader that could not tell a ROM from a text file.
3. ~~Stub `serial.c`.~~ **Done 2026aug28, and the framing was wrong.** Gert asked whether Droid48 even has serial. It does not: there is no serial, Kermit, XModem or terminal anywhere in its Java, resources or manifest; `useTerminal` and `useSerial` are hardcoded to 0 in `resources.c:84`; and the only thing that could raise them is `X48.ad.h`, x48's X11 app-defaults table, which nothing parses once the X11 frontend is gone. Droid48 compiles `serial.c` purely because it inherited x48's source list whole. So the file is already dead at runtime and needed no stub - it comes **out of the build** instead, joining `main.c`, `x48.c`, `debugger.c` and `disasm.c`, with its four entry points (`serial_init`, `serial_baud`, `transmit_char`, `receive_char`) as no-ops in `x48_shim.c` beside the X11 ones. Zero vendored-tree edits, and it is what lets the tree build for MSVC and MinGW, which have no `termios.h`, `ptsname_r`, `grantpt`, `unlockpt` or `O_NDELAY`. `receive_char()` must leave `interrupt_called` alone or `do_shutdown` (`actions.c:687`) reads a wake that never happened - which is what the real one does with `wire_fd == -1`.
4. ~~Fill in `kKeys`.~~ **Done 2026aug28.** See item 6.
5. `x48_take_beep`: `device.c` raises the beep through the Java audio pull that is no longer compiled; trap it at the OUT-register write instead.
6. `x48_reload_state` for the sync-conflict path.

### What next-step 2 turned into, 2026aug28: four `exit()` calls and a loader that trusted anything

The item read "turn `read_rom`'s `exit(1)` into an error return". `read_rom()` was innocent - it already returns 1 or 0 correctly. The `exit(1)` was one line above it, in `init_emulator()`, and it was not alone.

Every `exit()` reachable from the compiled tree is now gone:

- `init.c` - `init_emulator()` on a bad ROM, and `read_files()` on a failed RAM `malloc`.
- `serial.c` - `exit(-1)` when `ptsname_r` fails. Unreachable while `useTerminal` is 0, which is the default, but a library must not end the process. It now gives up on the wire and returns.
- `errors.c`'s `fatal_exit()` and `options.c`'s `usage()` needed nothing: Droid48 had already commented out the body of the first, and the second is only reachable from the command-line parsing in `main.c`, which is not built.

`init_emulator()`'s contract could not be repaired in place. It returns 0 for success, has no failure return at all, and two of its three exits are indistinguishable. So the logic moved to **`agape48_init_emulator()`**, which returns `NULL` on success or a reason string, and `init_emulator()` stays as a shell around it keeping its historical "always returns 0". That also means a careless re-vendor produces a link error in the shim rather than a silently inverted success test - the same property the `agape48_` prefix buys everywhere else in this tree.

It can distinguish the two failures because `read_rom_file()` sets `*mem = NULL` before doing anything and only assigns on success, so a non-NULL `saturn.rom` after a failed `read_rom()` means the ROM loaded and the RAM allocation is what failed.

**A real upstream bug, found on the way.** `romio.c:90` read `else if (four[1] = 0x49)` - an assignment, not a comparison, so the branch is always taken. Every file that did not match one of the two known magics was announced as "an HP49 ROM" and sized at twice its length. Fixed to `==`.

**And the loader still trusted almost anything.** With that typo fixed, the `else if (four[0])` catch-all accepts any file whose first byte is non-zero as a raw dump, so a text file or a JPEG was loaded and then executed as Saturn code. Size settles it without any guessing: the smallest real HP 48 ROM is the SX at 256 KB packed, which is 524288 nibbles, so anything below that or above 8 MB is rejected. Measured after the fix - a text file, an empty file, a 3-byte file, a 200 KB random file and a file with a zero first byte all now fail with a message instead of exiting or running garbage. 512 KB of random bytes is still accepted, and always will be: a raw dump carries no magic, and nothing short of a checksum separates it from noise. It runs two million instructions of nonsense with the display off and does not crash, which is the right outcome for an unrecognisable ROM.

## 2. DECISION: 1 Hz idle tick

`Agape48Engine::tick()` stops the timer when `x48_is_asleep()` is true. On a phone that is not optional - a 60 Hz timer that never sleeps is a battery complaint. But the HP 48 has real timers: the system clock, alarms, and the timer2 interrupt. If emulation stops for four hours of wall clock and then resumes, the calculator believes four hours did not happen.

- Advance the Saturn timer registers by elapsed wall clock on wake. Correct, and it is what the HP 48 hardware effectively does; needs care in the shim.
- Keep ticking at a very low rate (1 Hz) instead of stopping. Simpler, still cheap, keeps the clock roughly honest.
- Ignore it. Alarms silently do not fire, which is a real Droid48-parity regression.

**Decided by Gert on 2026aug28: the 1 Hz idle tick.** Cheap, keeps the system clock roughly honest, and avoids the battery complaint a never-sleeping 60 Hz timer would earn. Wall-clock catch-up on wake stays available if alarms turn out to matter in use.

## 2b. REQUIREMENT: read *and* write `HPHP48-` objects, both directions

Not an open question - a decision Gert made on 2026aug26. Agape48 must both import and export HP 48 binary objects, the format whose first seven bytes are the ASCII header `HPHP48-`.

This is the state-integration story. It is what makes a calculator portable between Agape48, Emu48, Droid48, x48 and a real HP 48 over Kermit, and it is the format every one of those already agrees on.

Droid48 reads it and cannot write it: its loader (`read_bin_file`) imports, and its only save is its own x48-format state files. **Emu48 reads *and* writes it** - `File > Load Object...` and `File > Save Object...`, sections 9.1 and 9.2 of its own help - but Emu48 runs only on Windows. So the gap is not that nobody writes the format; it is that no single program does both everywhere, and on Android nobody does both at all. Doing both on all three platforms is what makes Agape48 the tool people use to move between the others.

**Corrected 2026sep01, from the Windows laptop.** This paragraph used to read "none of them writes it", which was wrong about Emu48 and contradicted its own next clause. The REQUIREMENT is unchanged - Gert's decision of 2026aug26 stands - but the reason for it is narrower than it was written, and the old wording should not have gone anywhere public. Dogfood windows-05 proved the interop in both directions against Emu48 1.6.4, including a third-party library round-tripped byte-identically apart from the revision letter.

Note the trap in Droid48's loader, and do not copy it: when the `HPHP48-` header is absent it silently wraps the file's bytes as an HP 48 *string* object and pushes that instead of failing. The load reports success and the user gets something useless. Agape48 must reject a bad header loudly.

## 2c. DECISION: Droid48 everywhere, plus object export

Decided by Gert on 2026aug27, superseding an earlier draft of this section that had the desktop following Emu48. Emu48 counts only where it reproduces something Droid48 already does, and in practice that is nothing extra - so the rule is simply Droid48 on all three platforms.

**The whole UI spec is Droid48's menu.** Six items, from `X48.java:252-257`: show/hide minimal controls, save memory/state, put program on stack, settings, reset memory and quit, quit. Settings is Droid48's settings - contrast, back-key behaviour, ports, haptics, sound, large LCD, full screen, 48S mode, lock portrait. Windows and Linux get the desktop equivalents of the same gestures, not a richer shell.

**No Backup/Restore, because Droid48 has none.** Its persistence surface is the explicit "Save memory/state", the "Save on exit" setting, and "Reset memory and quit". There is no snapshot slot. The only `Backup` in the Droid48 tree is `DOBAK` in `rpl.c:78`, which is the HP 48's own backup *object* type and not an emulator feature.

**No serial.** Kermit and XModem are out for now; `serial.c` gets stubbed. This closes the question item 9 reopened. Object import and export cover moving things on and off the calculator.

**One state format everywhere**, x48's directory layout - `hp48`, `rom`, `ram`, `port1`, `port2`, with the `s`-suffixed set for the 48S. That was true under the previous draft of this section for sync reasons and it is trivially true now, since all three platforms run the same shell.

**KML leaves the binary on every platform.** With no Emu48-shaped desktop there is no reason to load Emu48 skins at all, so item 5 is answered as "neither option": no runtime parser, no build-time converter needed for shipping. `KmlParser.*` was deleted on 2026aug28; see item 5.

### The one addition: import and export to and from stack level 1

The single capability Agape48 has that Droid48 does not. Import already exists upstream as `read_bin_file` (`binio.c:204`); export does not exist in Droid48 at all, which is the 2b capability. Emu48 has both, on Windows only.

Export is smaller than section 3 assumes, because the hard part is already vendored. `RPL_ObjectSize()` (`binio.c:53`) walks composite objects recursively to get an object's nibble length. `Read5(DSKTOP)` (`binio.c:183`) gets the pointer to stack level 1. The `HPHP48-` header is eight bytes - the seven ASCII characters plus one revision char - which is why the loader starts the object at nibble offset 16. So export is: peek level 1, size it, read the nibbles, pack pairs into bytes, prepend the header, write. The only missing primitive is `Nread`, the inverse of `Nwrite` (`binio.c:132`), which is a few lines. Roughly 60 lines in total.

This also shrinks item 3, which was settled on 2026aug27: writing level 1 out as a *binary* file needs no object decompiler and no synthesised `→STR` keystrokes, and the text clipboard was cut down to numbers only, which needs neither either.

### Three traps in Droid48's import, not to be copied

- The silent string fallback on a missing header (`binio.c:258-266`), already described in 2b: a bad file loads "successfully" as an HP 48 string object.
- `DSKTOP`, `TEMPOB`, `TEMPTOP`, `RSKTOP` and `AVMEM` are hardcoded GX addresses in `rpl.h:49-54` with no SX variants, even though the same file defines both `ROMPTAB_SX` and `ROMPTAB_GX`. Object import in 48S mode therefore writes GX system addresses. The shim needs both sets and has to pick by model.
- `malloc(bin_size * 2)` at `binio.c:225` is unchecked, on a length taken straight from `st_size`.
- **Droid48 dropped two of Emu48's safety checks, and this is the serious one.** Emu48 bounds the object walk - `RPL_ObjectSize(lpBuf+16, dwSize-16)` - and validates the result with `if (dwSize == BAD_OB) return S_ERR_OBJECT;`. Droid48's `RPL_ObjectSize(BYTE *o)` (`binio.c:53`) takes no length bound and nothing checks its return. A malformed object file therefore walks off the end of the buffer, reachable from any file the user imports. Restore both the bound and the `BAD_OB` check when building import and export.

## 3. DECISION: the ROM formats and parses; Agape48 moves characters

Settled over 2026aug27-28 in several passes, each one cheaper than the last. The final shape: `DUP →STR` before copy, `STR→` after paste. The calculator's own ROM does the formatting and the parsing, because it is exact by construction and because it is free. Agape48 converts characters and moves bytes.

Both options this section originally weighed are dropped, and so is the middle path recorded on 2026aug27 of bounds-checking x48's per-type decoders.

### There is no `→STR` inside x48

Worth stating, because it is the obvious first assumption. `rpl.c`'s `dec_*` functions *are* x48's stand-in for `→STR`, written for the debugger - hence the surrounding double quotes on strings, the `\219`-style escapes from `hp48_trans_tbl` (`hp48char.h:47`), and the 1000-character truncation at `rpl.c:360-362`. The real `→STR` exists only in the ROM.

### Copy: let the user press `DUP →STR`

If `→STR` runs before copy, level 1 holds a string and copy handles exactly one type. That removes the whole job of threading bounds through 26 decoders, along with `dec_string`'s truncation, its quote wrapping, and the non-literal `sprintf` format string at `rpl.c:369`. Every decoder in `rpl.c` drops out of the picture except `real_number`.

Reading a string object is five nibbles of length followed by bytes: bounded by construction, no overflow to defend against.

It is also strictly more capable. `→STR` is total on the HP 48, so it covers the seven types `skip_ob` gives up on - Directory, Tagged, Graphic, Backup and External 2, 3 and 4. A grob yields `Graphic 131 × 64`. An earlier draft of this section called such a placeholder "fake data on the clipboard"; that was wrong, because it is not ours, it is what the calculator itself prints.

And it is safer. A large program hits the *calculator's* memory limit and raises an HP 48 error the user can see, rather than overflowing a C buffer inside the emulator.

The hint text must say `DUP →STR`, not `→STR`. `→STR` consumes the object, so a bare `→STR` replaces a program with its text and the program is gone. `STR→` on the paste side consumes the string and leaves the object, which is the wanted behaviour, so there is no equivalent trap there.

### The one exception: reals go direct

`real_number()` (`rpl.c:170`) stays, for `DOREAL` and `DOEREL` only. It is a pure BCD-nibble-to-decimal-text converter with no floating point in it, its output is bounded by nature at roughly 21 characters, and copying a number is the common case - requiring `→STR` for it would be irritating.

There is also a fidelity argument. `→STR` respects the display mode, so `1/3` in FIX 2 gives `.33`. `real_number` ignores FIX, SCI and ENG and gives all twelve digits. For a clipboard the value beats the rendering, because it pastes back as the same number.

Its output forms are `123`, `-123`, `.000123` and `1.23E-15`: trailing zeros stripped, no leading zero before the decimal point, never a `+`, never `E+`. The paste grammar below has to accept a bare leading `.` or copy-then-paste fails on the same machine.

So: reals and long reals direct, strings read directly, everything else asks for `DUP →STR`.

### Paste: number first, string otherwise

Text that parses as a number becomes a `DOREAL`. The grammar is deliberately stricter than `strtod` and matches what copy emits:

```
[ws] [+|-] ( digits [ sep [digits] ] | sep digits ) [ (E|e) [+|-] digits ] [ws]
       where sep is "." or ","
```

Reject if any character is left over. Round the mantissa at 12 digits. Reject exponents outside ±499. The integer-versus-floating-point distinction need not exist in the implementation: the HP 48 has one real type, so `5` and `5.5` take the same path.

**Decimal comma, system flag -48: liberal in, strict out.** Decided by Gert on 2026aug28. Paste accepts either `.` or `,` as the decimal separator; copy always emits `.` regardless of what the calculator displays, because `.` is what every other program expects and the clipboard's job here is interop. Neither direction reads flag -48, which was the declined alternative.

The grammar allows exactly one separator. Text containing both `.` and `,`, or more than one of either, is rejected - so `1,234.56` and `1.234,56` never become numbers. That rejection is not a failure: it falls through to string paste, where the user sees the text and can fix it. Getting the ambiguous cases out as visible text rather than as a confidently wrong number is the whole point.

One case stays ambiguous and is accepted as-is: a bare `1,234` is read as 1.234, not as 1234 with a thousands separator. Nothing in the clipboard says which locale the text came from, so this is unresolvable without flag -48. It is also harmless in practice - the wrong value lands on the stack in plain sight, immediately.

Do not route this through a `double`. `strtod` reports success on `12abc` because it stops at the first bad character, and it accepts `inf`, `nan` and `0x1f`. More importantly HP 48 reals reach ±9.99999999999E499 while a `double` tops out near 1.8E308, so `1E400` - a valid HP 48 number - would become infinity. The mirror of `real_number` is a digit shuffle rather than a numeric conversion: validate, then lay the decimal digits straight into 12 BCD mantissa nibbles, 3 exponent nibbles and 1 sign nibble.

Anything else becomes an HP 48 string object, and `STR→` compiles it if the user wants the object. That covers lists, matrices, algebraics, units, tagged objects and programs, using HP's own parser rather than one written here. Do not write a compiler: there is none in the tree, decompiling and compiling are not symmetric in cost, and a subtly wrong matrix is worse than no matrix.

**Normalise ASCII digraphs on the way in.** `hp48char.h` gives the codes: `«` is 171 and `»` is 187, escaped in `rpl.c` as `\<<` and `\>>`; `→` is 141, escaped `\->`; `≠` is 139, escaped `\=/`. A program copied out of a text editor as `<< -> x 'x^2' >>` will not compile without this. About twenty lines, and it adopts the escape convention the codebase already uses.

### Why neither conversion is automated

`STR→` evaluates: `"1 2 +" STR→` leaves 3, not a program. Auto-compiling on paste therefore means pasted text executes - harmless for a literal like `{ 1 2 3 }`, not harmless for a command sequence copied off a forum. Injecting `→STR` on the copy side has the mirror problem: it needs the calculator at a clean stack prompt, not in the EquationWriter, a menu, or mid-edit.

The manual version is both the safe option and the free one. If it ever nags in daily use, the escalation exists - `rpl.c:845-884` already walks `ROMPTAB` to resolve library entries for XLIB names, with `ROMPTAB_SX` and `ROMPTAB_GX` at `rpl.h:46-47` - but build it only if the keypress is actually missed.

### The character table is the one genuinely new piece

`hp48_trans_tbl` renders non-ASCII as backslash-decimal, which is debugger notation: a string containing `→` or `π` would reach a text editor as `\141`. Agape48 needs a real HP 48 to Unicode table for 0x80-0xFF, about 128 entries, and its inverse for paste. Cheap to write, easy to forget, and it silently produces garbage if skipped. Pasted UTF-8 with no HP 48 equivalent has to be rejected or substituted, deliberately rather than by whatever a lookup miss returns.

### Messages

A brief self-disappearing message, never the persistent error banner. In QML that is a `Rectangle` and a `Text` with an opacity animation and a `Timer`, needing no Qt Quick Controls, so the deliberate non-dependency holds.

- Paste that produced a real: no message.
- Paste that produced a string: "Pasted as text - `STR→` converts it".
- Copy with anything but a real or a string on level 1: "Press `DUP →STR` first".
- Copy with an empty stack, or paste with an empty or non-text clipboard, or too little calculator memory: the honest failure. `RPL_CreateTemp` already reports the memory case by returning 0 (`binio.c:160`), and that is the real size limit - there is no fixed character cap.

### What is left to build

The HP-to-Unicode table and its inverse, the number fast path in both directions, string nibble read and write, and the digraph normalisation. Nothing in `rpl.c` needs modifying, which is the point.

### Consequences in the current tree

- `x48_shim.h:117` and the `x48_text_to_stack` comment describe a general RPL-object clipboard. The seam handles reals and strings only, and the comments should say so.
- `Agape48Engine.cpp:316` reports paste failure through `setError()` and the persistent banner in `Main.qml`. These outcomes are transient and need their own signal.
- `copyStackToClipboard` (`Agape48Engine.cpp:293`) grows a 512-byte buffer by asking the shim for a required size. That contract is now easy to honour, since both remaining cases have a length known before writing.

## 4. DECISION: copy in, copy out

`StateFileManager::populateAndroidSaf()` hands the core four detached file descriptors. That works only if the vendored x48's `romio.c` is rewritten from `fopen(path)` to `fdopen(fd)`, and it assumes the fds stay valid for the whole session - they do not survive the user revoking the grant, and a document provider that restarts can invalidate them.

- **Copy in, copy out.** On start, read the state out of the SAF folder into app-private storage; on save/suspend, write it back. The core only ever sees ordinary POSIX paths, no upstream surgery at all, and the copy-back is the natural place to detect that the remote file changed underneath you.
- **fdopen surgery.** Zero copying, more invasive, more failure modes.

**Decided by Gert on 2026aug28: copy in, copy out.** The core only ever sees ordinary POSIX paths, `romio.c` needs no `fopen`-to-`fdopen` surgery, and the copy-back is the natural place to detect that the remote file changed underneath you - which is what the fingerprint in the README's sync section wants anyway. The detached-fd path in `StateFileManager::populateAndroidSaf()` is now dead and should be deleted rather than left as an alternative.

## 5. KML support may not be worth shipping in the binary

Emu48 skins are `.bmp` faces drawn for a 1990s desktop at LCD zoom 1 or 2. They load fine - `QImage` reads BMP with no extra module - but at 200-400 px wide they are unusable on a phone. So runtime KML support buys a large library of skins that mostly look wrong on the platform that matters most.

**Answered by 2c on 2026aug27: neither option.** With Droid48's shell on all three platforms there is no Emu48-shaped desktop to load Emu48 skins into, so KML support is not built - no runtime parser, and no build-time converter needed for shipping either. `KmlParser.*` was still *linked* despite that - listed in `CMakeLists.txt` and called from `SkinModel::loadKml` - so the parser that had been decided against was in the binary, text parser and all. **Deleted on Gert's instruction, 2026aug28**: `KmlParser.h`, `KmlParser.cpp`, `SkinModel::loadKml`, the suffix dispatch in `SkinModel::load`, and the `QFileInfo` include that only the dispatch used. 290 lines. `SkinModel` now loads JSON and nothing else. The `kScanMap` table went with it - it was dead, and filling in its 49 Emu48 scancodes would have been wasted work. The options below are kept only as the reasoning that led there.

- **Runtime KML parser**, as written. Good for desktop, costs binary size and adds a text parser to the crash surface.
- **`tools/kml2json.py`, a build-time converter.** Skin authors convert once; the app ships one format. Removes `KmlParser.*` from the binary, which is the size goal, and the conversion is where upscaling and re-tracing has to happen anyway.
- **Both**, with the parser behind a CMake option that is off for mobile.

## 6. DONE 2026aug28: the key table is filled in and verified

Transcribed from the `buttons[]` array at `src/core/x48/x48.c:233` - x48's original 1994 table, 49 entries, whose 4th field is a packed code that `x48.c:386` decodes as `row = code >> 4`, `mask = 1 << (code & 0xf)`. Checked by script: 49 entries, no two keys share a `(row, mask)`, all nine rows used, masks `0x01`..`0x20`, with rows 1, 2 and 3 carrying the sixth key (SHR, SHL, ALPHA).

Not `buttons.h`. Earlier notes here named that file and were wrong - it holds nothing but `#include "bitmaps/..."` lines.

Two names had to be reconciled: x48's digits are `"7"`, `"8"` where skins say `"N7"`, `"N8"`, and its `"COLON"` is the third row's first key, which skins call `"QUOTE"`.

**ON stopped needing a special case.** The skeleton had `X48_KB_ROW_ON 8`, which is wrong twice over: 8 is a real row (B, C, D, E, F live there), and it made ON a sentinel the frontend had to remember to expand into nine calls. x48 does not work that way either - ON is code `0x8000`, and `x48.c:381` sets that *bit* in all nine rows. So the mask carries the meaning: `X48_KB_MASK_ON` replaces the sentinel, `x48_key_down`/`x48_key_up` fan it across the rows themselves, the row argument is ignored, and the frontend makes one ordinary call. No new API, and it matches the original exactly.

### Verified by doing arithmetic

Against the 48GX ROM through the C shim, with the row and mask values read straight out of `kKeys`:

```
1 2 3 ENTER   ->  stack level 1 shows 123
7 x           ->  stack level 1 shows 861
```

Two things learned while testing, both about *reading* the display rather than about the keys:

- The HP 48 right-justifies stack entries, so a value sits at the right edge of the 131-px line, not the left. Dumping the first 70 columns shows the `1:` label and nothing else, which reads as a dead keyboard.
- Rows 56-63 are the soft-key **menu bar**, not the command line. The command line takes over the bottom stack row, and the stack display shrinks from four lines to three when it appears.

A key must be held long enough for the ROM to scan it. Ten slices of 17000 instructions - roughly 40 ms of emulated time - is reliable; four is not, and twenty-five triggers auto-repeat. That is a real constraint on the frontend: a synthetic tap from a physical-keyboard event must hold the matrix bit across at least one ROM scan, not press and release within the same frame.

## 7. Licensing has to be settled before the first binary, not after

- x48 is GPL, so Agape48 is GPL. The source ships with the binaries either way.
- Statically linking Qt under the LGPL obliges you to let a recipient relink against a modified Qt. In practice: publish the object files, or publish the complete build recipe.
- **Answered on 2026aug28 by reading the headers.** The x48 core files carry "either version 2 of the License, or (at your option) any later version" - checked in `buttons.h`, `binio.c`, `rpl.c`, `emulate.c` and `main.c`, all Copyright (C) 1994 Eddie C. Dost. So they are GPL-2.0-**or-later**, and the "or later" is the recipient's choice: Droid48's GPLv3 `COPYING` licenses the combined Android app, and does not bind the core files Agape48 takes. Since item 1 discards the Java layer entirely, Agape48 need not inherit GPLv3.
- The app-store friction is moot regardless. iOS is not a target, and Google Play has hosted Droid48 under GPLv3 for years.
- **`binio.c` checked against Emu48 on 2026aug28: it is Emu48 code.** `read_bin_file` matches Emu48's `files.c` around line 1597 line for line, down to the comments (`// load as string`, `// String`, `// length of String`, `// data`) and the magic `0x02A2C`. Its `Npeek`, `Nwrite`, `Read5` and `Write5` primitives match the signatures in Emu48's `mops.c` exactly, and the `typedef word_20 DWORD` block at the top of `binio.c` is the adapter making Emu48's Win32-flavoured code compile against x48. Compared against `dgis/emu48android`, which vendors the Emu48 core.
- **No licence conflict, but the attribution is wrong.** `binio.c` carries "Copyright (C) 1994 Eddie C. Dost"; the code is Christoph Gießelink's and Sébastien Carlier's. Emu48's core ships under the same GPL-2.0-or-later terms as x48, so Agape48 may use it - but the header misstates authorship and the replacement should credit Emu48.
- **The ROM permission, read on 2026aug28. There is no primary text.** Every source is a paraphrase and they disagree on distribution, which is the point that matters.
  - hpcalc.org, which hosts the ROM: "Revision R ROM dump of the HP 48GX calculator. HP graciously began allowing this to be downloaded in mid-2000." The strongest statement for distribution, and it says mid-2000 rather than fall.
  - Emu48's FAQ: "In general all HP-ROM files are copyrighted by Hewlett Packard. But since fall 2000 HP ACO allowed the use of the HP38, 39, 40, 48, 49 ROM's even if you're not an owner of this calculator type." Permission to *use*; silent on distribution.
  - Emu48's manual, same author, going further: "Because there's no license for the distribution of the ROM images, they aren't included in the Emu48 package."
  - Droid48's `rom_legal` string is Emu48's FAQ sentence with "non commercial" inserted, a phrase that appears in neither Emu48 source. Droid48 bundles both ROMs anyway - confirmed present in the built APK as `assets/rom` at 524288 bytes and `assets/roms` at 262144 bytes. The hpcalc.org URL at `X48.java:51` is only a comment recording their origin.
  - HP's ACO was shut down, announced 1 November 2001 and effective the 9th. There is nobody to ask and no successor has restated the grant.
- **Consequence: the earlier claim here overstated the case.** "Agape48 can bundle one as long as the release is free" is not supported by any permission text, and the non-commercial condition traces only to Droid48's paraphrase - the weakest of the sources. Bundling is defensible as a *risk judgement*: Droid48 has shipped both ROMs on the Play Store for years and hpcalc.org has hosted them openly since 2000, with no visible objection. It is not defensible as a permission.
- **Decided by Gert on 2026aug28: bundle both ROMs, the 48GX and the 48SX, as Droid48 does.** Taken knowingly as a risk judgement rather than as a permission, on the grounds that the exposure for an open-source non-commercial release is negligible - twenty-six years of hpcalc.org hosting them openly and years of Droid48 on the Play Store, with no objection from anyone.
- Bundling both is also what makes the 48S setting kept by 2c actually work; Droid48's `s`-suffixed state set (`roms`, `rams`, `port1s`, `port2s`) needs the SX ROM present.
- Size cost, measured 2026aug28 from Droid48's APK: 524288 bytes of 48GX ROM deflate to 339471, and 262144 bytes of 48SX deflate to 194075 (re-measured with `gzip -9` on 2026aug28: 329369 and 190737). So **about 510-530 KB compressed, 768 KB on disk after install**. That is comparable to everything the non-Qt levers in the README's size budget achieve combined, so it belongs in that table's reasoning rather than as a surprise later. Qt's `.qrc` compresses, so the desktop figure is the same.
- **Bundling the ROM alone does not produce a working calculator. Measured 2026aug28.** Droid48's `assets/` holds six files, not two: `rom` + `ram` + `hp48` for the GX and `roms` + `rams` + `hp48s` for the SX. The `ram` files are a pre-initialised 128 KB / 32 KB memory image and the `hp48` files are a 372-byte saved x48 config with the `HP48` magic. Droid48 does not cold-boot a blank machine on first run; it restores a canned warm state.
  - That is load-bearing, not a convenience. With the ROM present and no `ram`/`hp48`, the core runs about 1200 instructions, enters SHUTDN at PC `0x19fe` and never leaves - `saturn.intenable` is cleared by the first timer interrupt and only `RTI` or a SHUTDN with a zero `OUT` register restores it, and neither happens. Pressing ON does not help. With the seed present the same build reaches Memory Clear in 176760 instructions and ON dismisses it.
  - So Agape48 bundles six files. The ROM stays read-only wherever it is installed and is read in place, per next-step 1; `ram` and `hp48` are mutable and must be *copied into* the state directory on first run, which is `StateFileManager`'s job and matches the copy-in/copy-out of item 4.
  - Size cost of the seed over the ROMs alone is nothing: the RAM images are mostly zeros and deflate to 2594 and 1365 bytes, the configs to 218 and 216. **About 4.4 KB compressed, 164 KB on disk.**
- A photorealistic face of an HP calculator is trade dress, and "HP 48" in a store listing is a trademark use. Droid48 has lived with this for years, so the risk is evidently low, but it is not zero. Keeping the app name Agape48 and describing it as a Saturn/RPL calculator emulator costs nothing.

## 8. Two brief items contradict themselves

- "Use basic QSoundEffect" and "do not include Multimedia" cannot both hold: `QSoundEffect` *is* Qt Multimedia. Resolved in README under "Sound and haptics" - a small per-platform `Feedback` singleton, no Qt Multimedia.
- "60 fps LCD" and "smallest possible footprint / phone battery" pull against each other. Resolved by drawing only on change: `x48_take_frame()` returns false when the HP 48 drew nothing, which is most frames.

## 9. Smaller things worth knowing

- Emulating on the GUI thread is a deliberate choice (a 4 MHz interpreter is a rounding error on a modern core). The cost is that a long QML relayout or a JS garbage collection stalls emulation and jitters the beep. Acceptable for a calculator; revisit only if measured.
- `LcdItem` currently allocates a fresh `QSGTexture` on every changed frame. Reusing one `QSGPlainTexture` and calling `setImage()` avoids the churn. Small, worth doing.
- `MultiPointTouchArea` with `mouseEnabled` gives desktop exactly one touch point, so `ON + A + F` on desktop only works through the physical-keyboard path. That makes the `Qt.Key` mapping table load-bearing rather than a convenience.
- `-Os` on a Saturn interpreter dispatch loop can cost real throughput. `AGAPE48_CORE_OPTIMIZE_FOR_SPEED` exists for that, and it is worth one measurement rather than an assumption - on a phone, `-Os` may genuinely win on battery even if it loses on cycles.
- **Size target, stated by Gert on 2026aug28: under 20 MB installed.** "As small as it will go" had been read here as counting kilobytes; it was not meant that way. The static-Qt configure in the README is the only lever that decides whether the target is met - LTO, ICF, `--gc-sections`, WebP-versus-PNG and the plugin exclusions are all rounding against 20 MB. The practical consequence is to stop trading clarity or work for a few hundred KB, and to revisit the exclusions that were justified only on size.
- **First such revisit, decided by Gert on 2026aug28: Qt Quick Controls is allowed.** `Qt6::QuickControls2` is off the forbidden list in `CMakeLists.txt` and added to `find_package` and `target_link_libraries`. That returns `QtQuick.Dialogs`, a real `FolderDialog` on desktop, and ordinary text fields, in place of hand-rolled `TextInput`-inside-`Rectangle` work in `SettingsSheet.qml`. The aesthetic objection survives as a usage rule: Controls stays off the calculator face, which remains raw Qt Quick. Pin the style to Basic.
- ~~Droid48 also does serial file transfer (Kermit/XModem).~~ Settled by 2c on 2026aug27: no serial for now, `serial.c` gets stubbed, and the `HPHP48-` import and export in 2b covers moving objects instead.
- **Decided by Gert on 2026aug28: one 1080-wide portrait face, rotated for landscape.** No separate landscape photograph. Rotation is the known-imperfect option and is accepted as such - it can be revisited by shooting a second face later without changing anything in the code, since `SkinModel` already keys off the layout rather than the image.

## 10. DECISION: desktop key remapping, capture-by-pressing

**Proposed by Gert on 2026aug28**, for Windows and Linux: make the physical-keyboard mapping user-editable, in a simpler way than Emu48's. Ctrl+right-click on a key on the face opens a dialog for that calculator key. The dialog lists the physical keys currently assigned to it, each with a (-) to remove, and a (+) at the end of the list to add one. Adding puts the dialog into capture: the user presses a key, the dialog shows it, in red if it is already assigned to another calculator key, with Save greyed out until they pick a different one. ESC cannot be captured, because it is the dialog's Cancel; ESC is permanently ON.

A calculator key can carry several physical keys, which is the point - `Enter` and the keypad's `Enter` both want to be ENTER, `Backspace` and `Delete` both want to be BS.

This is right, and better than Emu48, which puts scancode numbers in a KML text file. Four things it leaves open.

### The seam already exists

`Keypad.qml:91` already calls `Agape48Keymap.nameFor(event.key, event.modifiers)`, and that singleton is not written yet. So the whole feature is one QML singleton plus a dialog; nothing in the engine or the shim changes. It sits **on top of** the default table from item 6, which still has to be filled in first - remapping is a layer over a working default, not a replacement for one.

### 10a. Conflicts: block, or steal?

Gert's design blocks - red text and a greyed Save. It is safe, and it re-creates the friction it is meant to remove: to move `A` from the calculator's A key to ALPHA, the user must cancel, open A's dialog, delete the binding, reopen ALPHA's dialog, and press A again. Three dialogs for one change.

Every current keybinding editor - IDEs, browsers, games - steals instead: "`A` is currently ENTER. Reassign?" One click, and the old binding is dropped. With the dialog's own undo it is not destructive.

**Decided by Gert on 2026aug28: offer the steal.** The capture row names the current owner - "`A` is currently ENTER" - and the user takes it with one click. Nothing is silently dropped, and the three-dialog detour is gone. Keep the red for the moment before the choice is made, so the conflict is still visible; it is the greyed-out Save that goes.

### 10b. Modifiers, and the trap under them

Unaddressed in the proposal, and it decides the shape of the stored map. If the user presses Shift+7 in capture, is the binding "7 with Shift" or just "7"?

**Decided by Gert on 2026aug28: a key with a modifier is a totally different key.** `(Qt::Key, modifiers)` is one opaque identity and the lookup is a plain hash - no most-specific-match rule, no fallback, no precedence to get wrong. It is the simplest rule and the most predictable one: you bind exactly what you pressed, and you must press exactly that.

The one consequence to know rather than argue with: a bare `A` binding does **not** fire while Shift or Ctrl is held, so typing with Caps Lock on will look like the map is broken. That is the rule working as decided, and it is also what makes layouts needing a modifier for digits work correctly - on AZERTY, `1` is Shift and capture records the chord actually pressed.

The HP 48's own shifts are sticky presses on SHL and SHR, not held modifiers, so none of this collides with how the calculator itself shifts.

**The trap underneath is that `event.key` is keyboard-layout dependent.** Shift+7 is `Key_Ampersand` on a US layout and something else on ABNT2. A map made on one machine will not match on another with a different layout - and Agape48's sync is a folder the user's own client watches, so a settings file *will* travel between the Windows and the Linux laptop. **So the keymap file is machine-local and stays out of whatever gets synced.** It is a personal convenience, not state. `event.nativeScanCode` would be layout-independent, but it is unreadable in the file and buys nothing once the file does not travel.

### 10c. ESC is two separate rules, and one of them should be visible

"ESC cannot be captured" is forced by the dialog using ESC as Cancel. "ESC is always ON" is a different rule, and a good one: it guarantees the calculator can always be switched on, no matter what the user does to the rest of the map. But if it is invisible, the user opens ON's dialog, does not see ESC listed, and wonders.

**Decided by Gert on 2026aug28: show it as a greyed, non-removable first row in ON's list.** The rule then explains itself, and ON is guaranteed to keep at least one binding no matter what the user does to the rest of the map.

### 10d. Nobody will find Ctrl+right-click

It is a good gesture and a bad only-entry-point. **Decided by Gert on 2026aug28: pair it with Settings, "Customize keyboard...",** which enters a mode where the face highlights and a plain click opens the same dialog. Shares the dialog; costs a mode flag.

Two smaller things while writing it: capture mode must swallow every key except ESC, so Save and Cancel have to stay mouse-reachable; and the (+) row needs its own cancel, since ESC belongs to the dialog.

### Scope

Windows and Linux, as Gert scoped it. Android gets it free if it goes through the same singleton, which matters only for a tablet with a hardware keyboard - no reason to exclude it, no reason to build UI for it.


## 11. DONE 2026aug28: free desktop scaling, aspect locked

**Asked by Gert on 2026aug28:** make the desktop face fully scalable, NOHALO, so the calculator can be tiny or fill the screen. Then: pick a sensible default at first run from the monitor size, and keep the proportions unless the user holds Ctrl while dragging a window edge.

Free scaling: yes, and most of it is already there. NOHALO: no, and it is the wrong tool for both surfaces here.

### NOHALO is not available and would not help

NOHALO is a GEGL resampler - Robidoux's level-2 edge-preserving interpolator, what GIMP 2.10 uses. Qt has nothing like it: `QSGTexture::Nearest`, a bilinear `Linear`, and mipmapping, plus whatever a `ShaderEffect` is written to do. Vendoring GEGL is out on both the module list and the size budget, so having NOHALO means hand-writing it as a fragment shader.

It would not pay, because the two things being scaled want opposite treatment.

**The LCD must stay nearest-neighbour.** 131x64 blown up to 800 px is a 6x upscale, and the HP 48's pixels are meant to be crisp squares - that is what Droid48 and Emu48 show, and any interpolation is a downgrade. `LcdItem.cpp:68` already sets `QSGTexture::Nearest` with the comment "square pixels, always". NOHALO here would be actively wrong.

**The face is a photograph and is almost always being scaled *down*.** NOHALO is an upscaler; downscaling is not where it earns anything. The real quality problem when the calculator is made "really tiny" is aliasing - a downscaled photo shimmers and its key legends sparkle. The fix is mipmapping, which Qt has natively. `Calculator.qml:32` currently sets `smooth: true` and no `mipmap`, so today it will shimmer at small sizes. **Add `mipmap: true`.** It costs a third more texture memory and nothing on disk.

### The one case that is genuinely upscaling, and the honest fix

The face only gets upscaled if the source image is smaller than the window, which at 1080-wide portrait means a full-screen 1440p or 4K monitor. The fix is not a better resampler, it is a bigger source: a 2160-wide WebP face instead of 1080 costs roughly 1-2 MB instead of 300-600 KB. **Against the 20 MB target from item 9 that is not a decision worth agonising over** - it is the same recalibration that reopened Qt Quick Controls. Ship the face at twice the phone width and the desktop never upscales.

### Default window size at first run

No settings file means no stored geometry, so take it from `QScreen::availableGeometry()`: some fraction of available height - 70% is a reasonable start - clamped so the face is never upscaled beyond its native size. Written into the settings JSON on the first save, and thereafter the file wins.

### Aspect ratio locked, Ctrl to unlock

`Calculator.qml:14` already scales by `Math.min(width / faceSize.width, height / faceSize.height)`, so the face keeps its proportions no matter what the window does. Locking the *window* to that ratio is the new part, and Qt has no built-in aspect constraint - it means correcting in `onWidthChanged` / `onHeightChanged`.

Two things to know before building it:

- **Unlocked does not mean stretched.** With a single uniform `scale`, a window of the wrong ratio gives letterbox or pillarbox bars, not a distorted calculator. Non-uniform stretch would need the key hit rectangles scaled on two axes independently and would distort a photograph of a real object. Bars are the cheap and correct behaviour; if Gert wants actual stretching, that is a different and worse feature.
- **Aspect-locked resize fights a Wayland compositor**, which owns the resize interaction rather than the client. This laptop is Cinnamon on X11 so it will look fine here, but it needs testing on Windows and on a Wayland session before the behaviour is trusted.

### Built on 2026aug28

- **`Calculator.qml`** - `mipmap: true` on the face `Image`, beside the existing `smooth`. `smooth` alone is bilinear on the full-size texture and does not stop a downscaled photograph aliasing; mipmaps do.
- **`Main.qml`** - the old `width:`/`height:` bindings are gone. A binding on either fights the user's drag and snaps the window back mid-resize, which is what the file did before. Geometry is now chosen once, in `applyDefaultGeometry()`: a stored size if there is one, otherwise 70% of `Screen.desktopAvailableHeight` clamped to the face's native height. It defers until `SkinModel` emits `changed()`, because the skin decides the ratio and sizing from the 480x900 fallback would never be corrected.
- **`Main.qml`** - `keepAspect()` on `onWidthChanged` / `onHeightChanged`, guarded by a `fixingAspect` flag against the obvious recursion, suspended while Ctrl is held.
- **`Agape48Engine::keyboardModifiers()`** - `QGuiApplication::queryKeyboardModifiers()`. This is the part that is not obvious: while a window edge is being dragged the window manager owns the interaction and **no key event reaches the application**, so the Ctrl override cannot be driven from `Keys.onPressed`. It has to read the live physical state.
- **Persistence** is `Settings` from the `QtCore` QML module, which maps to `QSettings` and is therefore per-machine by construction - the right home for geometry and, later, the keymap from item 10b. Backed by `Qt6::Core`, already linked, so no new module. A note in `CMakeLists.txt` records that a static build has to import its QML plugin.

None of it is compile-checked; there is still no Qt on this laptop.

### The face image is now the limiting factor

The placeholder is 480x900 (`assets/skins/default/layout.json`), which is why the fallbacks in `Main.qml` are 480 and 900. Item 11's own conclusion - ship the face at twice the phone width so the desktop never upscales - means a real face at 2160 wide.

Note that `layout.json` coordinates are in face-image pixels, so swapping resolutions means multiplying all 49 key rectangles and the LCD rect by the ratio. That is a scripted pass, not a re-trace, so it does not force the resolution to be decided first.

**Where the image comes from is a licensing question, not a technical one.** Item 7 already records that a photorealistic face of an HP calculator is trade dress. A photograph found online adds the photographer's copyright on top of that, and unlike the ROM there is no "graciously allowed" story to point at. The clean answer is to photograph a real HP 48 we own: no licence question at all, and a modern phone camera clears 2160 wide easily. Wikimedia Commons is the one online source worth checking, because CC-BY-SA is compatible with a GPL project given attribution - but Commons photos tend to be three-quarter views rather than the flat-on shot a skin needs.

## 12. Real photo, small keys: separate the hit rectangle from the drawn key

**Noticed by Gert on 2026aug29:** a real photo of an HP 48 is inconvenient in Droid48 because the buttons come out smaller than the placeholder's.

He is right, and it is arithmetic rather than taste. An HP 48GX is 92 mm wide; a phone screen is around 70 mm. A faithful face is therefore about 0.75x life size, and a key that is 9 mm on the real machine lands at roughly 7 mm on the glass - under the ~9 mm minimum every touch guideline uses. Emu48 and Droid48 both hit-test the exact printed key, which is why they feel fiddly.

**The fix costs nothing and needs no second image: let the hit rectangle be bigger than the drawn key.** The face stays photorealistic; `layout.json` gets an optional `"hit"` per key, defaulting to `rect`, drawn large enough that the hit areas tile the whole keypad with no dead gaps between them. `Keypad.qml:22` already hit-tests against `keys[i].rect` and `Keypad.qml:73` draws the press highlight from the same rect, so splitting them is a one-field change in `SkinModel` plus one line in each of those two places.

This is a strictly better answer than shipping a touch-optimised second skin, which would fork the layout and break the one-face rule from decision 2c. It is also why the real photo does not need to be shot with exaggerated keys.

Not built yet - it belongs with the real face image, not before it.

## 13. DONE 2026aug29: the first Linux build

The whole Qt side had never been compiled. Five things were wrong, all in the skeleton:

- **`x48_config_t` was undeclarable.** `x48_shim.h` defined it as an anonymous `typedef struct { ... } x48_config_t;` while `StateFileManager.h` forward-declared `struct x48_config_t;`, which does not exist. Both structs now carry tags.
- **`Agape48Engine.h` forward-declared `SkinModel` and `StateFileManager`** but exposes both as pointer `Q_PROPERTY`s. moc needs a complete type to build a QMetaType for a pointer property.
- **`SettingsSheet.qml` referenced `PathField`, `TextButton` and `Toggle`** - three components a TODO said would be written and never were, so the app refused to start at all. Replaced with `TextField`, `Button` and `Switch`, which is precisely what allowing Quick Controls on 2026aug28 was for. `QQuickStyle::setStyle("Basic")` in `main.cpp` pins the style as that decision required.
- **`LcdItem.cpp` double-freed the LCD texture.** It called `delete node->texture()` and then `setTexture()`, with `setOwnsTexture(true)` already set - and `QSGSimpleTextureNode::setTexture()` deletes the old texture itself. The render thread segfaulted on every frame. This is the bug that would have been hardest to find later, because it needs a GPU and a running window; no amount of C-side testing reaches it.
- **`LcdItem.cpp` also passed `f.stride` before the first frame**, when it is zero, and `QImage` with a zero stride is null - which then made `createTextureFromImage()` return null and `setTexture(nullptr)` crash. Guarded, with a stride fallback.

Two smaller things: `Screen.desktopAvailableHeight` can be 0 before the window is mapped, so `applyDefaultGeometry()` falls back to `Screen.height` and floors at 400px; and `Agape48Engine::start()` now falls back to a file called `rom` in the state folder when no ROM has been picked, which is x48's and Droid48's own layout and is what makes the app usable before decision 7 bundles a ROM.

**The WebP plugin is missing from the aqt Qt on this laptop** - `~/Qt/6.12.0/gcc_64/plugins/imageformats/` has gif, ico, jpeg and svg only - so the placeholder face was converted to PNG to unblock dogfooding. 9.5 KB as WebP against 60 KB as PNG, which is inside the "do not discuss a few kb" line from item 9. The real face should still be WebP; `aqt install-qt linux desktop 6.12.0 linux_gcc_64 -m qtimageformats --outputdir ~/Qt` is what adds the plugin.

Result: a 402 KB binary that opens a 403x756 portrait window, boots the 48GX ROM, and draws Memory Clear with the soft-key menu bar over the placeholder keypad. Report at `~/Dropbox/Claude/Agape48Emulator/Agape48-dogfood-linux-2026aug29-08h20.txt`.

### Dogfood #1, 2026aug29: eleven of thirteen passed

The two failures were both window geometry, and both were more interesting than they looked.

**The window was 160x300 on Gert's machine and 403x756 on mine, from identical code.** `Main.qml` had `visible: true` and sized the window afterwards, so the window manager was free to map it at a size of its own choosing - and `keepAspect()` then dutifully "corrected" whatever the WM picked and locked that in, with `geometryApplied` already true so nothing ever restored the intended size. Two window managers, two answers. Fixed by computing the size while the window is still invisible and showing it only once both dimensions are set.

This is the class of bug that only a second machine finds. Nothing about the code looks wrong on the machine where it happens to work.

**The size was not remembered.** It was written in `onClosing`, which never fired on the dogfood machine. It is now written 400 ms after the last resize, so a drag does not hit QSettings every frame and closing is not part of the contract at all. Verified both ways here: a fresh start gives 403x756, and a stored 277x520 comes back as 277x520.

**Decided by Gert on 2026aug29: drop the Ctrl unlock from item 11.** He expected unlocking to squash the calculator, saw that it could only ever add letterbox bars, and concluded the mode bought nothing. The aspect ratio is now always locked, `Agape48Engine::keyboardModifiers()` is deleted, and `QGuiApplication::queryKeyboardModifiers()` goes with it. The whole 10d-style "query, do not listen" note survives only as history here.

### Where the face image comes from, answered 2026aug29

Gert asked whether Droid48's images can be used. **They can, and they are the right ones.**

Droid48 has no photograph. `app/src/main/res/drawable/` holds 196 PNGs: 49 keys in four resolution tiers (`k`, `l`, `m`, `n`). The `m` tier is the largest - 42 keys at 156x108, the six menu keys at 156x66, and ENTER at 312x108 - and they are properly drawn HP 48 keys with their white, cyan and purple shift legends, not screenshots.

Composited, the `m` tier gives a keypad about 936 px wide and 930 px tall, so a full face lands near **1000 x 1500**. That is essentially the 1080-wide portrait target from the face decision, reached without photographing anything.

And it answers item 12 by construction. Droid48's proportions are wider and shorter than a real HP 48 - 0.67 against the real machine's 0.47 - because the keys are drawn large and the bezel small. That *is* the "convenient buttons" property Gert noticed; it is not an accident of the photograph, it is the reason Droid48 feels better to type on.

**The licence consequence is real and worth stating.** Droid48's `COPYING` is GPLv3. Agape48's core is GPL-2.0-or-later, and item 7's conclusion was that Agape48 *need not* inherit GPLv3 because it discards the Java layer. Taking these drawables reverses that: the combined work becomes GPLv3, and the images need attribution to Droid48's authors. That is a fine outcome - Google Play has hosted Droid48 under GPLv3 for years - but it should be a decision rather than a side effect.

The work is a scripted composite of 49 tiles into one face plus a regenerated `layout.json`, not a re-trace.

### The menu did not exist, 2026aug29

Gert could not find it because there was none. Decision 2c makes Droid48's six-item overflow menu the whole UI spec and none of it had been built - and `SettingsSheet`'s only trigger was `onRomRequired`, which stopped firing the moment `start()` learned to find a ROM beside the state. That was a regression introduced the same morning: the sheet went from rarely reachable to completely unreachable, and nothing failed loudly to say so.

Now: a `Menu` with Settings, Save memory now, Copy stack, Paste, Reset memory and quit, Quit. Right-click anywhere opens it, and a small raw-QtQuick corner button opens it too - the item 10d lesson, that a gesture nobody can discover is not an entry point. The right-click `MouseArea` takes only `Qt.RightButton`, so left clicks fall through to the keypad untouched.

Two of Droid48's six are absent because the features are: "show minimal controls" has nothing to show, and "put program on stack" is 2b's object import.

### Does per-key art simplify hit detection? No - it simplifies producing the rectangles

Asked by Gert on 2026aug29 about the Droid48 drawables.

Hit detection itself does not change and should not. `Keypad.qml:6` uses one `MultiPointTouchArea` over the whole face rather than a `MouseArea` per key, deliberately: per-key areas serialise touches, and ON+A+F needs three keys live at once. So the test stays "which rectangle contains this point", whether the art arrives as one composited face or 49 separate images. Compositing is also better for rendering - one texture rather than 49 scene-graph nodes.

What gets much simpler is **generating** those rectangles. The current 49 are a placeholder grid, and the plan for a photograph was to trace them by hand - `layout.json` still names a `tools/tracekeys.py` and a click-through session that were never written. With per-key tiles the compositor knows each key's exact size and position because it is the thing placing them, so `layout.json` falls out of the same script, pixel-exact, with no tracing and no misalignment class of bug at all.

Item 12 gets easier for the same reason: inflating each rect until it meets its neighbours is arithmetic on a generated grid, rather than eyeballing gaps over a photograph.

## 14. SUPERSEDED same day: the face was composited from Droid48's key art

**Decided by Gert on 2026aug29: use the Droid48 images.** `tools/makeface.py` composites `res/drawable/m01..m49` - the largest of Droid48's four resolution tiers - onto a 984x1468 body it draws itself, and writes `layout.json` in the same pass.

**It does not reimplement Droid48's layout.** `HPView.java` computes key rectangles at runtime through a long chain of per-index special cases and a `Matrix` per key; reproducing that faithfully would be fiddly and would buy nothing. What matters is the logical grid, and that is legible from the `centers` label array in the same file: four rows of six, then five rows of five, with ENTER double-width at index 24. The image sizes confirm the mapping - six 156x66 files at the front for the menu row, one 312x108 at index 24, the rest 156x108 - so drawable *m01..m49* are simply indices 0..48 in that order.

Rows 0-3 place at native 156 px. Row 4 is ENTER at 312 plus four at 156, which is also exactly 936. Rows 5-8 are five keys spanning the same 936, so each is 187 px and the art scales 1.2x - which is how a real HP 48's lower rows are laid out, wider keys and fewer of them.

**Item 12 is settled by construction rather than by a new field.** Each key's hit rectangle is its whole grid cell. The drawn key sits inside its tile with its own padding, so the hit areas already tile the keypad with no dead gaps and every target is larger than the key it draws. No optional `"hit"` needed, and nothing had to be exaggerated.

**And the rectangles are generated, not traced.** The `tools/tracekeys.py` and click-through session that `layout.json` used to promise were never written and are now never needed: the script that places the keys is the script that knows where they are.

Licence: Droid48 is GPLv3, so the combined Agape48 is GPLv3 and the art needs attribution - which reverses item 7's conclusion that Agape48 *need not* inherit GPLv3. Attribution is in `layout.json` and the script header. Flagged to Gert to confirm rather than drift into.

## 15. DONE 2026aug29: the physical keyboard works at all

`Keypad.qml:91` had called `Agape48Keymap.nameFor()` since the skeleton and the singleton was never written, so no key on the keyboard did anything - ESC included, which is how Gert found it.

`qml/Agape48Keymap.qml` is that singleton. The binding identity is `(Qt.Key, modifiers)` as one opaque value, per the decision in item 10b, with one necessary exception written into `nameFor()`: Shift, Ctrl and Alt report themselves as modifiers *while being pressed*, so a bare binding for them could never match its own event. They are special-cased; everything else requires `NoModifier`.

The letter mappings follow the alpha legends printed on the keys - A-F are the menu keys, G-L the MTH row, M-R the `'` row, S-X the SIN row, Y and Z on `+/-` and EEX. The face shows them, so the keyboard does them. ESC is ON, which is the binding item 10c wants shown greyed and non-removable in the remap dialog.

This is the default map item 10 edits. The dialog is still to come.

## 16. DONE 2026aug29: the art is ours, drawn rather than borrowed

**Gert, after dogfood #3: "Maybe we can do new art? Seems so simple..."** It was, and it was the right call - `tools/makeface.py` now draws all 49 keys instead of compositing Droid48's PNGs, which supersedes item 14 a few hours after it landed.

Three things fell out of one change:

- **The licence question disappears.** Nothing of Droid48's ships. Agape48 keeps the GPL-2.0-or-later of its x48 core, item 7's conclusion stands unreversed, and there is nobody to attribute for pictures. The legends are the HP 48GX's own printed labels - facts about the machine - and the text is rendered at build time, so no font is redistributed either.
- **Every art complaint in dogfood #3 became a one-line edit.** Borrowed key art is a fixed picture: "the numbers are too big" and "the gaps are too wide" were unfixable with it. Drawn keys make every dimension a constant at the top of the script - cap height, gap, band height, font sizes, colours, LCD zoom.
- **A pressed state became possible.** Per-key PNGs would have needed a second copy of all 49.

### What dogfood #3 asked for, and what it cost

- Numbers too big - centre labels now auto-fit their cap with a smaller ceiling for short strings.
- Gaps too wide - `GAP` is 8 px in a 1080-wide face, and the shift legends sit in their own band above the cap rather than forcing the keys apart.
- The screen should keep the pixel grid's proportions - the glass is the 131x64 pixel area plus a uniform 14 px, and the bezel now hugs the glass instead of spanning the body, which is what made the old one look like a small screen in a big black frame.
- The stray glyph beside ON - gone, because we choose what to draw.
- A pressed look - `layout.json` now carries `cap` per key, the drawn key inside the hit rect, and `Keypad.qml` lights *that* rather than `rect`. Lighting the hit area would draw a halo around the key instead of lightening it.
- **The gaps should press nothing.** This reverses item 12's "hit areas tile the keypad with no dead gaps": Gert asked for only a 4 px frame around each key. So `rect` is the cap plus 4 px and the space between keys is dead, which is his call and is what the dead space is for on a real keypad.

The two shift keys are blank violet and green arrows rather than glyphs, which is what the machine has.

`tools/makeface.py` writes `layout.json` in the same pass, so the rectangles still cost nothing and still cannot drift out of alignment with the picture.

### Slimmed on request, 2026aug29

Gert, before filling dogfood #4: make the buttons less wide, leave only a very small bevel between the LCD and the window edge, make it all slimmer - and then "there was too much unused real estate".

The change that made this tractable is that **the face is now sized from the LCD outwards** rather than the other way round. `FACE_W` derives from the glass, its bezel and one `EDGE` constant, and the keypad shares that same inset instead of sitting in a wider margin of its own. That is also how a real 48GX is built: the black display area runs nearly edge to edge and the keypad is inset inside the same line.

854x1430 and ratio 0.597, from 1080x1564 and 0.691. Caps are 131x80 - taller than wide is what reads as slim; squat caps are what made the first attempt look like a tablet.

Two things fixed on the way: long shift legends (SYMBOLIC, LIBRARY, PICTURE) ran over their neighbours once the caps narrowed, because each legend was fixed at half a cap - a legend now gets the whole cap when it does not have to share it. And the QML menu button was printing itself through the middle of "48GX", so the branding moved left of the corner.

This is the argument for drawn art in one paragraph: every one of those was a constant, and there are eighteen of them at the top of `tools/makeface.py`. With borrowed key PNGs none of it would have been possible at all.

### Narrower keys without dead space, and the settings exposed, 2026aug29

Gert: the keys can still be less wide, and now their text is too small.

Those pull against each other only if narrowing means shrinking the caps inside a fixed pitch and leaving margin at the sides - which he had already rejected as unused real estate. **The gap widened instead of the margin.** The column pitch still spans the full width, so the space moves between the keys rather than piling up at the edges, which is what a real 48GX has and what makes the keys read as narrow. The centre labels then grew: they only looked small because the caps around them were so wide.

**`tools/face.json`** now carries every number - LCD zoom, glass and bezel padding, the edge bevel, key gap, cap height, all four page margins, every font size, every colour. The script writes it with the defaults on first run and refuses to run on an unknown key rather than silently ignoring a typo. The two worth knowing are `gap`, which narrows the keys, and `cap_h`, which is what taller-than-wide means.

That file is the argument for drawn art made concrete. With borrowed key PNGs not one of these would have been adjustable at all.

### Droid48's own button constants, and the alpha letters that were one row out, 2026aug29

Gert, after dogfood #4: look closely at the relative dimensions and proportions of the buttons in Droid48, and the size and position of the text, and reproduce them with more fidelity. Also: the buttons there are all the same, with a gradient, which three SVGs of a button could reproduce and would look much fancier.

**Droid48 draws its buttons twice.** Once as the 49 fixed PNGs, and once parametrically in `HPView.drawButton()`. The parametric one is the honest source for "what are the proportions", because every number is written down instead of measured off a picture:

    gradient      LinearGradient(top -> bottom, 0xFF081021, 0xFF6B7173)   HPView.java:229
    border        black, 1 dp stroke                                      :567
    radius        5 dp                                                    :214
    cap inset     8 dp each side, 10 dp above, 5 dp below                 :207
    centre text   17 dp                                                   :512
    legend/alpha  11 dp, so 0.65 of the centre text                       :511
    left shift    0xFFBD92BD, right shift 0xFF73DFC6                      :505
    menu key      white fill inset 4 dp inside that same gradient cap     :236

That last line answers "the buttons there are all the same" precisely: **there is exactly one button in Droid48**, a gradient cap with a black frame, and the menu and shift keys are that same cap with a flat colour laid inside it. `key_cap()` now works the same way, and both callers pass a colour and an inset.

**Three SVGs would have been a step backwards, and this is why.** An SVG is a picture that has to be *scaled* to each key. Droid48 pays for that: its normal-key art is scaled by 1.1 in x for the six-column rows and drawn unscaled in the five-column rows, so the two families do not agree on how wide a cap is relative to its cell. A cap that is drawn at the size it needs has no scale factor to distort it, which is why the wide ENTER key and the short menu keys here keep the same corner radius and the same border weight as every other key. The gradient is nine lines of Pillow. The rasteriser an SVG would need is a build dependency we would have to carry to Windows and Android.

Where fidelity was NOT restored, deliberately: Droid48's cap is 0.60-0.73 of its column pitch, ours is 0.84, because Gert tuned that himself over two rounds and passed it. Droid48's digits are 0.63 of the cap height against 0.42 for its words; his dogfood #3 note was that the numbers were too big, so the ratio here stays near 1. Both are `gap` and `font_centre_short` in `tools/face.json`, one edit each.

**The bug the comparison turned up.** The alpha letters printed on the face started at MTH = A, so every letter was one row out from `qml/Agape48Keymap.qml`, which had them right: A-F on the six soft keys, G-L on the MTH row, M-R on the ' row, S-X on the SIN row, then Y on +/- and Z on EEX. The face said STO carried H while the H key pressed PRG. Dogfood #3 line 12 passed because it tested the keyboard against the real machine, and #4 line 9 passed because it tested that a letter was present. Nothing tested the two against each other. The face table was the wrong one and is now the machine's.

Also from the same read: legends are centred over the cell as one group with a third of the slack between them, rather than pinned to the cap's outer corners, which is why "RAD POLAR" is allowed to be wider than the MTH cap. The alpha letter moved off the cap to the bottom right of the cell, which is where the machine prints it. ON regained CANCEL under it.

### The pressed highlight never worked, and could not have, 2026aug29

Dogfood #4 line 5, FAIL: the key face does not react at all to a keypress.

Two independent causes, and the first is a QML trap worth writing down. `Keypad.qml` held `property var held: ({})` and wrote `held[p.pointId] = i`. That **mutates the object without ever assigning to the property**, so no change signal fires and any binding reading it is never re-evaluated. The highlight was computed exactly once, at zero. `onCanceled` did `held = ({})`, a real assignment, which is why that one path would have worked.

The second: `held` only ever knew about touch points, so a key pressed on the physical keyboard could not have lit up even with the binding fixed.

Both go away by making the engine the source of truth. `Agape48Engine::pressedKeys` is a notifying `QStringList` maintained in `pressCode`/`releaseCode`, which is the one place the name path and the direct-matrix path meet, so neither caller has to remember to do the bookkeeping. Keypad's `held` map stays, but only for its real job: routing a release to the key the finger started on. Verified under XTEST: a mouse press lights only the 7 cap, a physical 5 lights only the 5 cap.

### The keyboard dies the first time anyone opens Settings, 2026aug29

Dogfood #5 line 15: "all the keyboard keys stopped working, including ESC, but clicking them with the mouse works". Then, decisively: **the instance he left open stayed dead while a fresh one worked.** So it was accumulated state, not the build.

What the stuck instance showed, driven from outside with XTEST: X input focus was on its window, a mouse press lit its cap and moved the stack, and a keypress produced **no pixel change anywhere** - not even the pressed highlight, which sits upstream of the emulator. So `Keys.onPressed` was not firing. The global X modifier state was `0x0` and a fresh instance on the same server worked, which rules out `nameFor()` rejecting a stuck modifier, and there was no lingering popup window under its PID.

**Reproduced in four steps: open the menu, choose Settings…, click any text field in the sheet, close the sheet.** Every key is dead from then on, for the life of the process.

`Keypad.qml` declares `focus: true`. That is granted once, at creation, and never again. A `TextField` takes active focus when clicked; when the sheet hides, the field's focus is dropped and nothing gives it back. **There was no way out**, either: clicking the face does not restore it, because a `MultiPointTouchArea` never takes focus. One visit to Settings and the keyboard is gone until you restart.

Three layers now, because one would have been the same bet again:

- `Main.qml` has a `focusGuard` timer, kicked by `activeFocusItemChanged`, by the window becoming active, by `settings.onOpenedChanged` and by `appMenu.onClosed`. If neither the sheet nor the menu is open and the keypad does not have focus, it takes it back.
- The guard asks `calculator.hasKeyboardFocus()` rather than testing `activeFocusItem` against null. The first version tested for null and **still failed**: after the sheet hides, `activeFocusItem` goes on naming the hidden text field, so "has focus fallen on the floor" was the wrong question. "Does the keypad have it" is the right one.
- `Keypad`'s touch handler calls `forceActiveFocus()`. Clicking the calculator is now the user's own way back, which is what a desktop user will try first anyway.

Verified against the reproduction: dead at every step before, and after the fix the keyboard survives closing the sheet, closing the menu, and alt-tabbing away and back. Typing still goes to a focused text field while the sheet is open, which is the point of having one.

### 17. DONE 2026aug29: Settings is a window, and three of its controls were fiction

**Gert, dogfood #6 line 1: "it sincerely feels eerie that the settings is not in a normal separate window instead of on the face of the calculator. Why this strange choice?"**

The honest answer is that there was no choice, only an unrevisited default. `SettingsSheet.qml` was written phone-first, before a desktop build existed, and a sheet rising from the bottom edge is the Android idiom. The Linux build then arrived, dogfood #2 said plainly "this sheet doesn't fit in the window and looks like a bad idea, it needs its own dialog window", and it went onto the "next round" list in #3, #4, #5 and #6 rather than costing twenty minutes. Four deferrals is one too many; it is a `Window` now.

It was also the cause of the dogfood #5 focus bug. A separate window structurally cannot repeat it: its text fields take focus in their own window, and closing it reactivates the main window, whose `onActiveChanged` hands the keyboard back to the keypad. The guard stays anyway - the next thing to take focus will not announce itself either.

**Three controls were removed because they were not real**, which is a worse failure than the sheet:

- **Copy stack, Paste, Save now.** Gert's rule from the same note: they are already in the ⋮ menu, and one command belongs in one place.
- **Haptics and Beep.** Two `Switch`es over the `Feedback` singleton, which is still a TODO on every platform. They toggled a property nothing reads. He said "there are two toggles I have no idea what for" - the honest answer being that they did nothing at all.
- **Pick folder…**, found while rewriting. It calls `StateFileManager::requestLocation()`, which under `#ifdef Q_OS_ANDROID` runs the SAF tree picker and on desktop only emits `pickerRequested()` and waits for a `QtQuick.Dialogs FolderDialog` that no QML file has ever shown. Adding one means adding the Qt Quick Dialogs module against a lean-module rule, so it is a question for Gert rather than a change to make quietly. Until then, typing a path and dropping a folder on the window both work.

A fourth thing was broken rather than missing: `migrateTo()` takes a `QUrl` and rejects anything that is not a local file, so the old field passed it a bare path and silently did nothing. The window converts a typed path to a `file://` URL, and shows the current one as a path rather than a URL.

**Gert on the measurement line, dogfood #6: "(what do you get from this?)"** Nothing, in general. `ls -l` on the binary earned its place exactly once - his stuck instance was running pre-fix code and the timestamp was how to tell his build from mine. As a standing line it asks him to type something that tells us what running it already told us. A measurement line has to be able to fail; that one could not.

### 10 BUILT 2026aug30: Ctrl+click, and the menu icon as the only menu

**Gert, 2026aug30: "I believe it should use ctrl+click for each key. And the menu can be accessible only via the menu icon."** Two changes to the August design, and the second is what makes the first safe: with right-click no longer opening the ⋮ menu, the only click gesture the face answers to besides a keypress is Ctrl+click, and there is no question of the two being confused.

Built as `qml/KeyBindingWindow.qml`, with `Agape48Keymap.qml` rewritten from a fixed table into an editable one.

**Three departures from the August note, all recorded rather than assumed:**

- **No Save button.** Its only job in the sketch was to be greyed out while a conflict was unresolved, and 10a replaced blocking with stealing. With stealing there is nothing for Save to be out of step with - the list is the truth and every edit lands as it is made. Offered back to Gert if he wants it.
- **"Customize keyboard…" went into the ⋮ menu, not Settings** as 10d said. It is a command, not a setting, and Gert's own rule from dogfood #6 is that commands live in the menu. With right-click gone the ⋮ menu is also the only surface anyone can find.
- **Ctrl+click needs the live modifier state.** A `MultiPointTouchArea`'s touch points carry no modifiers at all, so the gesture cannot be read off the event. `Agape48Engine::keyboardModifiers()` wraps `QGuiApplication::queryKeyboardModifiers()`, which is the same call the aspect-ratio Ctrl override needed before it was dropped.

**Storage.** `Settings` from the QtCore QML module, category `keymap`, one JSON string of `"<key>:<mods>" -> name`. Machine-local by construction, which is what 10b requires: `event.key` is layout-dependent, so a map made here would be wrong on the Windows laptop, and the state folder is the thing that syncs between them. A default that the user deletes is *shadowed* with an empty binding rather than removed, or it would come straight back; an edit of the user's own is deleted outright.

**A bug the testing found, which is the argument for driving the UI rather than looking at it.** `openFor()` reset `capturing` and `pendingKey` but not `pendingOwner`, so reopening the dialog could show a conflict row left over from the previous time it was open - and its "Take it" then bound key `0`, a binding for a key that does not exist. It was only visible because the second run of the same script produced `{"0:0":"N7"}` in the settings file. Fixed, plus `steal()` now refuses a zero key.

Verified under XTEST end to end: right-click does nothing; Ctrl+click opens the right dialog and does not also press the key; ON shows the greyed `Esc` row; a free key binds at once; a taken one offers the steal; after taking 8 for the 7 key the physical 8 lights the 7 cap and not the 8 cap; it survives a restart; reopening shows no stale conflict; reset empties the map; Esc closes the dialog; and "Customize keyboard…" makes a plain click open the dialog instead of pressing the key.

### The three smaller things from dogfood #7

- **Esc closes the settings window.** Gert: "I'm thinking this program should be fully usable without a mouse." A `Shortcut` on `StandardKey.Cancel`, and the same in the binding dialog, where it cancels a capture first if one is running.
- **A "…" folder picker** beside the state folder field, which he asked for after seeing the field work. This is the open question from item 17 answered: the Qt Quick Dialogs module goes in, 98 KB of binary, native on this desktop because Qt ships the gtk3 platform theme. `StateFileManager::requestLocation()` and its `pickerRequested()` signal are still unused - the QML dialog is wired straight to `migrateTo()`.
- **The ROM field, which he asked about.** Agape48 ships no ROM and cannot (item 7), so it looks for a file named `rom` beside the state and the field points it elsewhere. What that buys is a different machine - 48SX, 48G, revisions, or a ROM with libraries already loaded. The face still says 48GX either way, which is a real limitation worth a decision if he ever runs a 48SX.

### 18. FIXED 2026aug30: moving the state folder produced a folder that could not boot

**Gert, before filling dogfood #8:** the app started with the settings window already open, and a red message was hiding behind it. He guessed it was about the ram folder, pressed Use app storage, and it went away.

Three faults, one visible symptom.

**The move copied the wrong files.** `StateFileManager` kept a single `kStateFiles[] = { "ram", "port1", "port2", "state" }` and used it for two unrelated jobs. As the Android SAF list it is positional against `{ fd_ram, fd_port1, fd_port2, fd_state }` and must not change shape. As the migration list it was wrong twice: the core names its config file **`hp48`**, not `state` (`x48_shim.c` sets `conf_filename`), and **`rom` was missing entirely**. So "move my memory here" copied `ram`, looked in vain for a file that never exists, and left the ROM and the CPU state behind. Split into `kSafFiles` (positional, with `state` corrected to `hp48`) and `kMigrateFiles = { rom, ram, hp48, port1, port2 }`.

This had been dogfooded and passed. Dogfood #7 line 12 asked "type a folder and press Enter, the memory moves there" and he marked it pass, noting "a ram file appeared there as soon as I typed something on the calculator". That was the *saving* working, not the *moving* - the test asked whether a file appeared, not whether the right ones did.

**The ROM path was never persisted.** `setRomSource()` wrote nothing to `QSettings`, so a ROM typed into Settings survived only that session. With the migration bug that removed the last way back: no `rom` beside the state, and no remembered path either. Now saved under `rom/source` and restored on start.

**The error announced itself where it could not be seen.** The failure raises `romRequired`, which opens the settings window, and the banner was anchored to the *top* of the calculator - exactly where that window opens. The one error that opens a window by itself posted its explanation behind it. The banner moved to the bottom and lasts 12 s instead of 6, and the settings window shows `engine.lastError` in a red strip of its own, which is where a person would look anyway. The message also names the folder now: "There is no file named \"rom\" in %1" beats "No HP 48 ROM selected."

**Debug logging**, which he asked for in the same breath: a switch in Settings, a `qInstallMessageHandler` appending to `<AppDataLocation>/agape48.log`, remembered across restarts. It logs the four facts that answer this class of question on sight - state folder, ROM source, whether a `rom` sits beside the state, and which of the five state files are actually there. **The log lives in app storage, never in the state folder**, for the same reason the keymap does not: the state folder is the one that syncs.

Reproduced and verified: before, moving a folder left only `ram`; after, `rom`, `ram` and `hp48` all arrive and the app restarts from it cleanly.

**One aside worth keeping.** The test script typed a path into the field and got `7` wherever `/` should have been - Gert's layout is Danish (`setxkbmap -query` says `dk,us`) and `/` there is Shift+7. That is the item 10b argument turning up in real life, and it is why the keymap file is machine-local. Type into a field under XTEST via the clipboard, not character by character.

### 19. FIXED 2026aug30: a Qt Shortcut is not scoped to the window it is written in

**Gert, dogfood #8 line 23: "Suddenly the ESC key seems to be not assigned to ON anymore."** He left it broken on purpose so it could be debugged rather than restarted away.

Measured, not guessed. A `console.warn` in `Keypad.Keys.onPressed` showed Escape arriving and mapping to ON before the settings window was opened, and **not arriving at all** afterwards, while `7` kept arriving throughout. So nothing was wrong with the keymap or with focus; something upstream was eating one key. Disabling the settings window's `Shortcut` alone, changing nothing else, brought Escape back.

**A `Shortcut` declared in a secondary `Window` grabs its sequence for the whole application, and goes on doing it after that window is hidden.** `enabled: root.active` does not fix it - that was tried and measured still broken. The fix is to not use `Shortcut` in a secondary window at all: an `Item` with `Keys.onEscapePressed` inside the window catches the key by bubbling up the focus chain and cannot reach past its own window.

The same trap had a second victim, reported in the same round as #8 line 18: the Customize-keyboard mode used a `Shortcut` for its own Escape, so once that mode was on, Escape did nothing in the binding dialog either. Moved into `Keypad.Keys.onPressed`, which is window-scoped by construction, with a `customizeCancelled()` signal up to `Main.qml`.

**Rule for this codebase: `Shortcut` only in `Main.qml`, and only for things that should work everywhere.**

### The rest of dogfood #8 and #9

- **Ctrl+right-click, not Ctrl+left** (#8 line 2). Gert asked for left on 2026aug30 and changed his mind after using it: the left button is what you press all day on the face, so Ctrl+left is one stuck modifier from an accident, while the right button does nothing else here. A `MultiPointTouchArea` never sees the right button, so the gesture needs its own `MouseArea` with `acceptedButtons: Qt.RightButton` - which also means a plain left click still falls straight through to the keypad.
- **The Fn key bound itself** (#8 line 8). His ThinkPad needs Fn for F12, and Fn arrives as `XF86WakeUp` → `Qt.Key_WakeUp` (0x010000b8), which capture recorded as a binding named "0x10000b8" - a key the calculator could never see again. `isBindable()` now refuses anything above 0x01000000 that is neither in `keyNames` nor a function key. `keyName()` was also widened from ASCII to Latin-1, because Æ, Ø and Å are ordinary letter keys on his layout and must stay bindable.
- **Removing a stolen key killed it** (#8 line 14 and his keymap file, which held `"56:0": ""`). `unbind()` shadowed whenever a default existed, so taking 8 for the 7 key and then removing it left 8 pressing nothing anywhere. The rule is now: a row **in** the overrides is deleted, so the default returns; a row that IS a default is shadowed. Plus `resetKey()` behind a "Reset this key" button.
- **A typed folder was created** (#9 line 2). `migrateTo()` called `mkpath()`, so a typo silently made a folder and moved into it. It now refuses a folder that does not exist and says to use the "…" button, which is where a folder should come from.
- **Migrating onto the same folder destroyed it** (#9, the empty `sharorder`). The copy loop removes the destination before copying, so with source == destination it removed each file and then failed to copy it from itself. A second Enter on an unchanged path wiped the calculator's memory. Guarded by comparing canonical paths first.
- **A failed migration said nothing.** `StateFileManager::setError` was read only by `start()`. The engine now mirrors it, so it reaches the banner and the settings window like any other error.
- **The ROM field showed nothing** (#9 line 24) even with a ROM plainly loaded, because `start()` filled `m_romSource` from the "beside the state" fallback without emitting `romSourceChanged()`. It emits now, and the field has a `FileDialog` behind a "…" of its own.
- **"Use app storage" became a house button** at the left of the state folder row (#9 line 31), with a tooltip.

### Two questions for Gert, from the same round

**Several instances at once.** He saw six Agape48 windows - five of them my uncleaned test copies - and asked whether a user could really have that many. Nothing stops it, and it is not harmless: two instances on one state folder overwrite each other's memory, because each saves the whole state on quit and the last one out wins. Neither a single-instance lock nor a state-folder lock is built. His call.

**The black bands** are not a bug and a title bar will not change them. While a window edge is being dragged the window is briefly the shape it was dragged to, and the face is drawn to fit inside it, so the remainder shows as bars until the aspect lock corrects the other dimension. Painting the window background in the body colour rather than near-black would make them invisible.

### A key nobody claimed now says so, 2026aug30

**Gert: "if the user types a keyboard key that's not assigned, there should be a 3-seconds error message too, showing which unassigned key was pressed and that it's not assigned."**

A key that does nothing and a calculator that has hung look identical from the outside, which is most of why the dead-keyboard bugs took two rounds to pin down. `Keypad` emits `unassignedKey(label)` when `nameFor()` comes back empty, and `Main.qml`'s banner grows a `hint()` flavour: neutral grey rather than error red, three seconds rather than twelve. The text names the key with its modifiers - "Ctrl+A is not assigned to any key" - and adds "Ctrl+right-click a key to give it one", so the message teaches the gesture that fixes it.

Two things it deliberately stays quiet about. **Auto-repeat**, so holding a key says it once. And **keys `isBindable()` refuses**, because Fn on his ThinkPad would otherwise complain every time it is used to reach F12 - a key the system keeps is not a key you failed to assign.

Measured: showing at 2.6 s, gone at 3.5 s.

The **house button** is drawn on a `Canvas` rather than set to "⌂", which in most fonts is a thin outline box with a lid and reads as anything but a house at button size. Roof with an overhang, walls, door. `Canvas` is part of QtQuick, so it costs no module.

### 20. ANSWERED 2026aug30: the screen that went blank was the calculator switching itself off

**Gert, dogfood #10 line 13: "The screen went blank, but it came back when I clicked on the title bar. Was it some kind of sleep mode? Please check if you see what took place in the log file."**

The log could not say, because it only ever spoke at startup. That is the first finding, and his to claim.

The cause is his own observation from line 5 of the same report: **Ctrl is bound to the right shift**, and Esc is ON. So **Ctrl+Esc is green-shift ON, which on a 48GX is OFF** - printed on the key. He had been holding Ctrl all afternoon for Ctrl+right-click, so an accidental OFF was one stray Esc away the whole time. Reproduced: Ctrl+Esc blanks the screen, any key brings it back.

Clicking the title bar looked like the cure because the window regaining focus calls `resumeFromBackground()`, and the tick had stopped too - `tick()` stops the timer whenever `x48_is_asleep()`, since the 48 spends its idle life in SHUTDN.

**Now logged**, when Debug logging is on: the screen going blank and coming back, SHUTDN entry, and the emulator stopping and resuming.

**The mistake worth keeping.** The blank check was written first against `frame.height`, which is 0 when `display.on` is false - and it never fired once. **OFF does not clear `display.on`**: it blanks the LCD buffer and drops into SHUTDN with the register still set. `x48_take_frame()` also returns false unless the pixel buffer changed, so a state change that leaves the buffer alone is invisible to the frontend by design. The check now scans the frame for a non-zero pixel, which is what the user actually sees, and costs 8 KB per changed frame only while logging is on.

**Ctrl as the green shift stays.** Gert: "I think I like it like this. Let's keep it like this, but let it be known." So: Ctrl+left-click presses a key green-shifted, Ctrl+right-click opens its dialog and presses nothing.

### An error message outlived its cause, 2026aug30

Dogfood #10 line 17: the "there is no folder called…" message stayed on screen after picking a folder that did exist. Nothing ever set `lastError` back to empty - `setError` was only ever called with a complaint. `migrateTo()` and `useDefaultLocation()` now clear it as they begin, a successful `start()` clears it, and the engine mirrors the state manager's clear as well as its errors. The mirror had been one-directional for the same reason: it only forwarded non-empty strings.

### 21. DONE 2026aug30: no title bar, and the key that was too quick to see

Two things, and the second explains a mystery from dogfood #11.

**A tap was not long enough to be a keypress.** #11 line 2 failed: Ctrl+Esc switched the calculator off and pressing Esc did not switch it back on, but clicking the title bar three times did.

The ROM polls the key matrix on its own schedule, about every 40 ms, so a key that goes down and up between two scans never existed. Survivable while the calculator is running - it is scanning anyway, and a human press lasts longer than one gap. **Fatal from SHUTDN**, because there the scan only begins once a key arrives, and a tap was over before the ROM looked. My own test had passed because XTEST held the key for 250 ms.

The title bar "worked" for a reason with nothing to do with title bars: activating the window calls `resumeFromBackground()`, the emulator runs a few slices, and the ROM finally notices the interrupt the earlier tap had left pending. Three clicks was how many it took for the timing to line up.

Fixed in `Agape48Engine`: every key is guaranteed `kMinHoldMs` (60 ms) down. `releaseCode()` defers a release that comes too early, `tick()` completes it once the time has passed, and the deep-sleep stop is held off while a release is owed. Verified down to a **10 ms tap**, faster than anyone can type. It also fixes dropped keys during fast typing, which nobody had reported yet.

**The log said "deep sleep" on every frame** the Saturn spent asleep rather than on the moment it fell asleep - and it falls asleep between every pair of keystrokes. Gert: "Looks like a mess." One line per transition now, in both directions.

**No title bar**, `Qt.Window | Qt.FramelessWindowHint`, as Emu48 has it.

- **Moving** goes through `QWindow::startSystemMove()`, exposed as `Agape48Engine::startSystemMove()`. Any press on the body that is not a key starts it, which is the Emu48 gesture; `Keypad` already knew whether a press hit a key, so it only had to say so. Handing the drag to the WM keeps snapping and workspace edges working.
- **Resizing does not**, and this is the interesting half. `startSystemResize()` was tried first and **measured**: the WM's resize is interactive and ignores the height `keepAspect()` assigns during the drag, so a 451x756 window came out 523x756, ratio 0.692 against 0.597. `Main.qml` drives the resize itself from the edge under the cursor, computing both dimensions from the face's ratio every frame. Exact on all four edges and both corners, tested.
- That also disposes of the **black bands** from dogfood #9: they were the window being briefly the wrong shape mid-drag, and it no longer ever is. The window background moved to the body colour as well, so anything that did slip through would not read as a hole.
- The unused `startSystemResize` invokable was removed rather than left as a decoration.

The moc trap from the very first build repeated itself here: a `Q_INVOKABLE` taking `QQuickWindow *` needs the complete type, not a forward declaration. Same error text, same fix.

### 22. DONE 2026aug30: the SHUTDN park, and Shift as a level selector

Dogfood #13. Three failures from #12, and item 21's account of the first one was wrong.

**A key pressed at a parked Saturn was delivered one step too early.** #12 line 11 still failed: Esc took three taps to switch a sleeping calculator back on. The 60 ms hold from item 21 was worth having and stays, but it was not the cause.

The ROM's wake-up code — the part that switches the display back on — is the code **after** the SHUTDN instruction, reached only when `do_shutdown()`'s own wake loop decides it has a reason to wake. `agape48_emulate_slice()` pumped `GetEvent()` at the top of every slice, which delivered the key to the interrupt handler at `0xf` instead; that runs, returns to the SHUTDN, and parks again. The key was seen, acted on, and got nowhere. Three taps was luck of timing, and the title bar "helping" was the same luck bought with extra running time.

`GetEvent()` is now skipped while `x48_parked`, so the SHUTDN gets the key. Measured: 8/8 wakes on a single 10 ms tap.

**A stopped emulator is not a sleeping calculator.** `tick()` used to `stop()` on SHUTDN. But the 48 leaves SHUTDN on a timer as readily as on a key, and with nothing running there were no timers: the clock froze whenever the machine was idle, which is nearly always, and alarms could not fire. It now falls back to `kIdleIntervalMs` (100 ms) and never stops. **Measured cost: zero CPU ticks over ten idle seconds.**

**`do_shutdown()` was returning without a wake reason.** Clearing `exit_state` ends its `while (wake == 0 && exit_state)` loop either way, and the PC is already past the SHUTDN, so the ROM ran on as though something had woken it. Once per key press that is invisible; ten times a second it is not, and a switched-off calculator woke itself after ~300 ms and painted the stack back on. The shim now puts the PC back on the SHUTDN whenever the pass found no reason to wake. `x48_shutdn_woke`, set at the end of `do_shutdown()`, is **the only edit in the vendored actions.c**.

These three are one bug wearing three hats, and the idle tick is what made the other two visible.

**`x48_take_frame()` skipped a frame in which only `display.on` changed**, because it compared `lcd_buffer` alone. `display.on`, `display.annunc` and `display.contrast` are part of the comparison now. That is also why item 21 recorded "the display on/off register never fires" and reached for a pixel scan instead — the register was fine, the frame carrying it was being dropped. The scan is gone: the ROM clears the whole screen before drawing a full-screen form, so for one frame an ordinary menu looked exactly like a calculator that had been switched off, and the log said so.

**The sleep logging is gone entirely.** Gert, #12: "Most of the deep sleep messages I got were while wide awake." True and correct — the 48 parks between every pair of keystrokes — but a line per keystroke describing the CPU idling is noise, not a log. What is left is `screen off` / `screen on`, one line per transition.

**Shift is a level selector, not a modifier.** #12 line 17: `*` could not be typed at all. On a Danish layout it is Shift+`'`, and on a US layout Shift+8; either way it arrived as "Shift+something" and nothing was bound to it. Same for `/` (Shift+7 on Danish).

A binding identity is now one of two strings, and `Agape48Keymap` is built around them:

- `"c:*"` — a character the keyboard produced, upper-cased, so `a` and `A` are one binding but `*` and `'` are two.
- `"k:16777220:0"` — everything with no character to it: Enter, arrows, F5, Shift. Qt.Key and modifiers, as before.

Ctrl and Meta stay real modifiers, so `Ctrl+A` is still its own key. The map is now the same on every layout, which it never was; Gert's rule of 2026aug28 (no fallback, no most-specific-match) survives where it still applies.

The other half is in `Keypad`: pressing Shift no longer presses SHL, or reaching for `*` would shift the calculator as well as the keyboard. The decision moves to the release — nothing typed in between and it was the calculator's shift, so it taps then; something typed in between and it was a level selector, and the calculator never hears about it. A release is also matched by `nativeScanCode` rather than by key, or letting go of Shift before `'` would release QUOTE and leave MUL held down for good.

**The jerky resize** (#12 line 4) was four window-geometry requests per mouse event — x, y, width and height each on its own, with the window stepping visibly through the shapes between. One `QWindow::setGeometry()` now, through `Agape48Engine::setWindowGeometry()`, because QML cannot reach the four-argument overload and `Main.qml` already has a `setGeometry(w, h)` helper of its own. That shadowing cost an hour: `root.setGeometry(nx, ny, w, h)` bound nx and ny to w and h and sized the window 60x100.

### 23. DONE 2026aug30: the annunciators, and one instance per state folder

**The annunciators were never omitted.** Dogfood #13 line 12: "the LCD indicators at the top are never lighting... their functions work but their lights are mute." The bits were right and always had been - `display.annunc` goes 0x80 -> 0x81 for left shift, 0x82 for right, 0x84 for alpha, 0x90 for busy, and `x48_take_frame()` translated them correctly. `Calculator.qml` drew a lit one as a `#101010` rectangle on a bezel painted `#121214`. Black on black, at full opacity, every time. Painting them red for one build put the block exactly where it belonged the moment Shift went down.

The face art drew nothing in those six slots either, so there was nothing when off and nothing distinguishable when on.

**They came from the machine, not from me.** x48 vendored the real HP 48 bitmaps and they are in this repo: `src/core/x48/bitmaps/ann_*.h`, 15x12 pixels each. Worth reading before drawing anything - the two shift annunciators are **not arrows**, they are solid blocks with an arrow knocked out in reverse video, which is not what anybody draws from memory (I had drawn plain arrows first). `ann_battery.h` is x48's name for what the machine calls the **alert** annunciator, `((•))`; the name is kept so the code matches the emulator it came from.

`tools/makeface.py` reads those headers and writes six PNGs into the skin, in the LCD's pixel colour, scaled NEAREST so they stay square like display pixels. Baked rather than drawn from a font at run time: an hourglass or a pair of exchange arrows is exactly what a phone font does not have, so this way all six look identical on all three platforms. `SkinModel` resolves the image per annunciator and the QML delegate is an `Image`, not a `Rectangle`.

**Placement**: inside the glass, above the dot matrix, where they are on the machine - not on the bezel above it. Size is `ann_zoom` in `tools/face.json`, now 2 against the LCD's 6. Gert on seeing 4: "don't use too much vertical space for them, look how it looks for Droid48." At 2 the face is 854x1438 against 1430 before, so the ratio moves 0.597 -> 0.594 and nothing else on the face shifts. `ann_w`/`ann_h` are retired; the glyph size follows the bitmap.

`x48_take_frame()` had to learn that `display.on`, `display.annunc` and `display.contrast` are part of "did anything change" - it compared `lcd_buffer` alone, so switching the LCD off changed no nibble and the frontend went on showing the last frame of a calculator that was no longer displaying anything. That is also why item 21 concluded "the display on/off register never fires" and reached for a pixel scan: the register was fine, the frame carrying it was being dropped.

**One calculator, one folder, one instance.** Gert's own scheme was a `subinstanceNNN` folder per running instance; he spotted the cost himself - it invents a second way of naming states on top of folders - and asked for something simpler. The state folder IS the calculator, so it is opened the way a word processor opens a document.

`StateFileManager::claim()` writes `en-uzo` (pid, host, start time). A second instance on this machine with a live pid is refused and `Agape48Engine::stateFolderBusy()` opens Settings. A dead pid is taken over in silence, because nothing is reading that folder. A lock from **another** machine is a warning, not a refusal: there is no way to ask a switched-off machine whether its copy is running, and refusing would lock the user out of their own calculator with no way back except deleting a file they do not know about. Local files only - an Android `content://` tree would need the whole SAF dance to write one small file, and Android will not run two copies of an app anyway.

Two things that only showed up once it was built:

- **`migrateTo()` copied first and asked afterwards.** Pointing a refused instance at a busy folder would have written over the other instance's memory before refusing. `isBusy()` is checked before a byte moves.
- **Being refused was a dead end.** The state folder box calls `migrateTo()`, which never retried `start()`, so choosing a folder that worked still left a dead calculator until the user quit and reopened. The engine now retries `start()` on `locationChanged` whenever it is not ready.

`release()` is on `QCoreApplication::aboutToQuit` rather than the destructor alone: `Qt.quit()` from the menu tears the QML engine down in an order this object does not control. A crash still leaves a lock, which is exactly what the dead-pid check is for.

**On testing.** Half of this evening's results were worthless: `cinnamon-screensaver` maps a full-screen window that swallows every XTEST event, so a healthy app looked completely dead - no key, no click, not even a hover highlight - while `_NET_ACTIVE_WINDOW` still named the right window. Everything here was redone on a nested `Xephyr :7` with a Danish layout, which the lock screen cannot reach and which does not fight the user for the keyboard. That is now the way to drive this app from outside.

## Deferred, asked for on 2026sep09 and deliberately NOT done yet

Gert, after an evening of speed measurements: *"the reason I didn't change the speed is because I could not see the control. (The toggles look too big, and the control is outside the settings window. But don't fix it now, just add a note to make it after. Along with make the 48GX underlined and get rid of the 3 dots, like the Android solution)"*

**1. The Slower/Faster row was off the bottom of the settings window, and that cost a whole evening.** He never changed the rate because he never saw that he could - every number of that session was taken at the default. The row was already cut down to one line in `9470842` and it was still out of reach, so shortening is not the answer: **the window is `min(520, fitW) x min(430, fitH)` and his calculator window is around 356x599, so the dialog is far shorter than 430 and most of the content is below the fold.** It scrolls, and a scrollbar he did not notice is the same as not being there. Whatever replaces this has to be judged at HIS window size, not at 520x430.

**2. The toggles look too big.** The Basic style's `Switch` is drawn for a phone. Three of them in a column at that size is most of what fits in his dialog, which is part of why anything below them is out of sight.

**3. The 48GX becomes the menu on the desktop too, and the ⋮ goes away.** Android already works this way, from his own idea on 2026sep07: the golden "48GX" on the face carries a golden underline so it reads as a link, and tapping it opens the menu. `Calculator.qml` has the underline and the `TapHandler` gated `Qt.platform.os === "android"`, and `Main.qml` gates the `menuButton` the other way. Ungating both is most of the work; the desktop also wants a hover cursor, which a phone has no use for.

Nothing here is a bug in the emulator and none of it blocks the speed work, which is why it is written down instead of done.

**4. A hover tooltip on every key, naming the keyboard keys bound to it.** Asked for on 2026sep10: *"When the mouse pointer halts for 2 seconds over a button on the calculator's face, each of it's assigned keyboard keys appears at a yellow tooltip, between angle brackets <>, and when they are more than one, separated by a newline."* So a 2 s hover delay, a yellow background rather than the style's default, each binding wrapped in `<>`, and one per line when a key has several. Desktop only in effect - a phone has no pointer to halt - which puts it beside item 3 as the second thing the desktop wants and Android does not. The bindings already exist in one place for the keyboard handler to use, so the work is reaching them from the face's buttons and not inventing a second table: **whatever is written must read the same source the key handler reads, or the tooltip will start lying the first time a binding changes.**

**5. The folder path in the Calculators window becomes a link that opens - or focuses - a file manager.** Asked for on 2026sep10: *"I also would like the folder path in the 'Calculators' window to be underlined and clickable, so that it will open in a new window of the default file manager when clicked, if there's no result after searching for an already open window in ANY file manager program showing this folder to bring it to focus."*

Two halves, and they are not equally buildable.

The opening half is one line - `QDesktopServices::openUrl(QUrl::fromLocalFile(path))` - plus the underline and a hover cursor, the same treatment item 3 wants for the 48GX.

**The focusing half is the one to be careful about, and the first thing to do is find out whether it is needed at all:** Explorer already tends to raise an existing window that is showing the folder rather than open a second one, so `openUrl` may satisfy the whole request on Windows for free. **Measure that before writing anything.** If a search really is needed, the only reliable source on Windows is the `Shell.Application` COM object's `Windows()` collection, where each open Explorer window reports a `LocationURL` that can be compared as a path. For *any other* file manager - Total Commander, Directory Opus, the new Files, and every Linux one - there is no equivalent: no standard window property carries the folder being shown, so the only thing left is matching window titles, which is unreliable and is the same class of mistake as addressing a window by a title substring. So **"ANY file manager program" is not deliverable as stated**; what is deliverable is the platform's own file manager plus a graceful fall back to opening a new window. Say that rather than quietly implementing the fragile version. Android has no pointer, no default file manager to speak of and no window to focus, so this is desktop-only like items 3 and 4.

**6. Pace by Saturn cycles rather than by instructions.** Asked on 2026sep10: *"Maybe we should change the core emulator to expose the processor's clock ticks instead of operations?"* This is the right unit and the current one is a fudge. A real 48GX is specified in cycles, not instructions - Emu48 stores `GXCycles 123` cycles per timer2 tick, which is 123 x 8192 = **1,007,616 cycles a second**, a physical constant of the machine - and that is why Emu48's authentic speed reproduces to twelve digits. Pacing by instructions can only ever be right for one average instruction mix, so a program whose mix differs runs at the wrong speed by however much its mix differs.

**The cost is the reason it is not done here.** The vendored x48 core carries no cycle counts anywhere: `emulate.c:2216 instructions++` is the only counter in it, and the budget unit throughout the shim and `agape48_emulate_slice()` is one `step_instruction()`. Giving it cycles means a cycle count for every opcode in the interpreter - hundreds of them, each needing a source for the number - and every one that is wrong is a speed error nobody will be able to see. So: correct in principle, a large and verifiable-only-in-bulk job in practice, and worth doing only once pacing by instructions is measured and known to be the limiting inaccuracy. Which it is not yet: as of 2026sep10 the same loop varied 6x run to run for reasons that had nothing to do with the unit.

**7. A "Keep running when out of focus" toggle.** `qml/Main.qml:982` halts the emulator whenever the window goes inactive - `onActiveChanged: ... else engine.suspend()`, and `suspend()` is `stop()` plus `saveState()`. Gert found this on 2026sep10 by watching a 200-sample program: *"It runs ONLY when the calculator in in focus!!!"* and then, having thought about it, *"Maybe it's not a bug, but a feature. I can leave with that. Maube it could be a new settings toggle, 'Keep running when out of focus'."* So: **his decision is that the current behaviour stays the default**, with a toggle to defeat it.

Measured while he had it unfocused: 4881 ticks x 16 ms = 78.1 s of running against 443.4 s of uptime - alive for **17.6% of the elapsed time**. Anything that runs long on the 48 (a game left to think, a long program, `PAUZ`) is therefore paused by alt-tabbing away, which is right for a phone and arguable on a desktop.

Two things whoever builds this must keep:

- **`suspend()` also saves.** The save on losing focus is what makes the state-folder handover safe and is how this session has been reading his stack at all. A toggle must keep saving on unfocus and only skip the `stop()`.
- **It cannot simply be a third switch in Settings.** Deferred item 1 is that the dialog already overflows his window with what it has; adding a fourth row without fixing that hides something else. Items 1, 2 and 7 are one job.

**On item 7, he settled it further the same morning:** *"Although it's totally ok to have games progress only when focused."* So suspending on deactivation is **wanted**, not tolerated, and the toggle is a convenience rather than a fix. Nobody should "repair" `Main.qml:982`.

**And on whether the throttle needs a warm-up allowance: it does not.** Measured over the 200-sample run of 2026sep10, in run order: the first 20 clean samples mean 0.51505 s with an 11.4% spread, the last 20 mean 0.53967 s with a **1.2%** spread, total drift **+4.8%**, settled by about sample 37 - roughly **19 s of running**. His concern was *"I wouldn't like games to behave like this"*, and at under 5% for the first twenty seconds it is below anything a player can perceive. The factor-of-two "warm-up" claimed earlier in this project was the broken timer ruler settling, not the pacer.

## Done on 2026sep10, and the one that is not

*"Can we implement and build this fix and all of the deffered features now..."* — `d951f60`. Six of the seven, and the list above is left as written rather than edited, because what was asked for and why is worth more than a tidy checklist.

| item | state |
|---|---|
| 1. Speed controls unreachable | **done** — moved to the Advanced window, which is where he asked for them |
| 2. Toggles too big | **done** — `CompactSwitch`, sized from `TextSizes.dialogBody`, spacing 10 -> 6 |
| 3. 48GX is the menu, ⋮ gone | **done** on every platform, verified on a screenshot |
| 4. Key tooltips | **done** — 2 s, `<brackets>`, one per line, dark on yellow |
| 5. Folder path a link | **done for the openable half** — see below |
| 6. Pace by Saturn cycles | **NOT DONE, deliberately** — see below |
| 7. Keep running when unfocused | **done**, off by default |

**Item 5 is half-delivered and it is the half that exists.** `Qt.openUrlExternally` opens the folder; the "search ANY file manager for a window already showing it" half was not written, because there is nothing to write it against. Only Explorer reports the folder it is displaying (`Shell.Application`, `Windows()`, `LocationURL`); every other manager on Windows and all of them on Linux would leave title matching as the only option, which is unreliable. **Explorer's own behaviour now decides** whether an already-open window is raised. If it turns out to open a second window on a folder already showing, that is the moment the search earns its complexity - and not before.

**Item 6 is not attempted, and the reason is not effort.** The vendored x48 core carries **no cycle counts at all** - `emulate.c:2216 instructions++` is the only counter in it, and the budget unit throughout the shim is one `step_instruction()`. Giving it cycles means a sourced cycle count for every opcode in the interpreter, and there is no such source in this tree. **A guessed table would silently undo a rate that is now accurate to 0.8%**, measured over 200 samples against a twelve-digit Emu48 reference, and the error would be invisible - which is the worst kind. It stays deferred with its own section above, and the honest precondition is: get a real Saturn cycle table first, then change the unit, then re-measure against the same loop.

**What the new slider does NOT touch.** `speedFactor` is a multiplier over `realSpeedRate`, not a replacement for it. The rate is the measurement - three evenings, two independent routes, 4.05 cycles per instruction as the corroboration - and the slider must not be able to destroy it, so 1.0 in the middle always restores authentic speed exactly. If a future session finds the slider "redundant" with the rate, read this paragraph before merging them.
