# Agape48 0.9.3

The release where choosing a ROM actually works — on the phone it never did — and where the menu became a window of its own.

## The ROM you choose is now the ROM that runs

**Settings → HP 48 ROM** let you pick a file and then quietly ignored it. On Android that had been true since the field existed: the path showed the document you had chosen and the calculator went on with the ROM it already had. Three faults, all silent, stacked on top of each other.

The picker handed back a *document* rather than a path. The copy of that document into the calculators folder ran only when no `rom` was there yet — so the first choice on a fresh folder worked and every later one was thrown away. And a ROM changed under a running calculator never reached the emulator at all, on any of the three systems, because the start it asked for returns immediately when a calculator is already running.

So the field is now read as it is typed, and as soon as anything lands in it from the picker. The rule is the emulator's own: the four bytes at the front of the file and the nibble at `0x29`, which is exactly what the core reads before it will run a ROM. A path that leads nowhere and a file that is not an HP 48 ROM say **different** things, because that is the question you have when it fails. The field turns red — which also covers a ROM deleted or moved after it was loaded — and **Save** cannot be pressed while it is red.

**Close is gone from Settings; Save and Cancel are there instead.** The picker only fills the field. Save is the one thing that loads a ROM, and it does it in the same window, without the program restarting: a 48GX becomes a 48SX where it stands. Cancel puts back the path the core really opened, so a path typed, thought better of and cancelled is not a setting.

Checked against every HP 48 ROM on hpcalc.org — `gxrom-k`, `l`, `m`, `p`, `r` and `sxrom-a`, `b`, `c`, `d`, `e`, `j` — and all eleven are recognised, five G/GX and six S/SX. Checked also against a ROM cut in half, a ROM with three bytes added, a text file, a file of zeros and a 40-byte fragment; all five are refused.

## A new calculator on an SX ROM no longer takes Agape48 down

With no card in the calculator's ports, `saturn.port1` and `saturn.port2` are null — and the 48SX ROM configures both slot controllers regardless. A new calculator on `sxrom-j` read there while the display refreshed during start-up and died with SIGSEGV: five starts out of five on Linux, and on Windows at the first keypress. An empty slot now reads as zeros, which is what an empty slot is.

## The menu is a window of its own

It used to be a sheet over the calculator's face, and it could be drawn off the edge of the screen. It is now a real window that stays inside the screen wherever the calculator sits, carries no taskbar or panel button, and closes when the calculator loses the focus — including on Android, where the window is never told it lost anything and the whole app going to the background is the signal instead.

**Three looks**, in Advanced. *Screen look* is green like the calculator's screen, in a black frame, with OPTIONS MENU in dots on a dark band — square corners, like the pixels it imitates. *Button look* is dark blue, lighter towards its right and bottom edges. *Classic* is the colour of the calculator's body. The choice is remembered.

Every item now carries two characters of space at both sides of its text, measured in the menu's own font, and the sliders in Advanced are the size of the switches beside them rather than desktop-sized.

## The keyboard, the clipboard and the screen

**Ctrl+C** copies the stack and **Ctrl+V** pastes, with no phantom shift arrow left lit afterwards. **Right-click on the calculator's screen pastes**, and **Ctrl+right-click copies** — the screen is the stack, so that is where the stack's clipboard belongs. A key with no binding says so instead of doing nothing quietly.

**A click on the calculator's body that does not move it no longer blocks 48GX.** The drag handler was swallowing the release, so the next click on the menu went nowhere.

**The face shows 48SX** when an S-series ROM is loaded, in the same size and colour as the 48GX it replaces, read from the ROM rather than from a setting.

**Windows opened from a button open inside the screen** — About, the shelf, the keyboard editor, Settings and Advanced — wherever the calculator has been dragged to.

## New means new, and Reset memory means reset memory

**New** on the shelf used to copy the calculator you had open: same stack on the screen, `ram` byte for byte identical. That was a workaround for an empty folder ending at "Try To Recover Memory?" with no key getting past it — which was the empty-card-slot crash above, and is fixed. A new calculator is now empty, and the ROM builds its memory from nothing, exactly as a real HP 48 does after ON+A+F.

**Reset memory and quit** did not reset any memory. It asked the core for a cold reset, which was never implemented and fell through to a warm one, and then the ordinary save on the way out wrote the unchanged memory back to disk. On Linux the calculator came back with its stack; on Windows a warm reset from the middle of an instruction left the memory confused. It now puts the core down without saving and removes the four files that *are* the calculator, so the next start builds memory from nothing and asks the 48's own question.

## Drag and drop, and the HP 48 character set

A file or a piece of text that the calculator can load can be dropped on it, and the cursor shows the plus only when the load can actually happen. Text arriving from outside is converted through the HP 48's own character set, and a string that is not an object is wrapped with `→STR` rather than refused. On Android every hover is accepted and the address is looked for in all the forms the system hands it over in.

## Everything else is 0.9.2

Same file formats, same calculators, same folders. Android upgrades in place; your calculators stay where they are.

**No ROM ships with it** — it is HP's code, it is a free download from hpcalc.org, and the [README](https://github.com/gbmaizol/Agape48/blob/main/README.md#then-give-it-a-rom-and-this-is-the-only-fiddly-step) names the exact file and what to do with it.

## What is still missing

The Android build has never been measured for speed, and on a tablet Agape48 still cannot put *itself* into a floating window — Android does not let an app do that. Those are what `1.0.0` waits for.

## Licence

GPL-3.0-or-later, as before. The x48 core is Eddie C. Dost's, vendored and not forked; the face, the frontend and the Qt glue are this project's.
