# Agape48

**The HP 48 you already love, on the computer and the phone you actually use — and it takes your calculator with you.**

"HP" spoken in Brazilian Portuguese is *agá-pê*, which is the Greek ἀγάπη: love. Agape48 is named after the way people talk about this calculator.

---

## If you have never used an HP, read this part first

Almost every calculator you have ever touched works like this:

```
( 3 + 4 ) × 5 =
```

You had to type two parentheses to tell it what you meant, and you found out whether you meant it at the very end. Get one of them wrong and the answer is wrong, quietly.

An HP 48 works like this:

```
3 ENTER 4 + 5 ×
```

Say what you have, then say what to do with it. That is all RPN is. **You never type a parenthesis again**, and the calculator shows you every intermediate result as it happens — after `4 +` you can see `7` sitting there, so a mistake is visible the moment you make it instead of at the end.

It reads oddly for about twenty minutes and then it reads like arithmetic. Three things happen after that:

- **You stop counting keystrokes** — RPN is usually fewer of them, and never more.
- **You stop restarting.** Everything you have typed stays on a stack, visible, in front of you. Made a mess of the last number? It is one key to drop it, not a fresh start.
- **You stop trusting a single line.** A long calculation on a normal calculator is one number and one hope. Here it is four numbers you can look at.

If you want the short version: type the numbers, press `ENTER` between them, then press the operation. That is enough to do real work on day one.

## Why this one

There have been HP 48 emulators for thirty years, and they are good — Agape48 is built on the core of one of them. What there has never been is *one* of them that behaves the same way on your laptop and your phone, and hands the calculator back and forth between them.

| | Windows | Linux | Android | reads HP 48 object files | writes them |
| --- | :---: | :---: | :---: | :---: | :---: |
| **Agape48** | **yes** | **yes** | **yes** | **yes** | **yes** |
| Emu48 | yes | — | — | yes | yes |
| Droid48 | — | — | yes | yes | — |
| x48 | — | yes | — | — | — |

That last pair of columns is the one that matters in practice: an HP 48 object file is how a program or a matrix moves between all of these and a real calculator, and until now no single program would both read and write them everywhere. On Android nothing would do both at all.

## Your calculator, not a copy of it

The calculator's memory is **a folder you choose**. Put that folder in Dropbox, OneDrive, Syncthing, a memory stick — anything that syncs files — and the calculator you were using on the laptop is the calculator that opens on the phone, with your variables, your programs and your stack exactly where you left them.

Two honest details, because this is the part other emulators leave you to discover:

- **One at a time.** A calculator's memory is a heap with pointers into itself; two devices editing it at once cannot be merged by anybody. So Agape48 puts a marker in the folder saying which machine has it open, tells you when another one does, and **says so rather than quietly picking a winner**. It is the same deal a password database gives you, and for the same reason.
- **It really is just files.** No account, no cloud of ours, nothing phoning home. Your sync client was already good at this; Agape48 does not try to replace it.

You can keep **several calculators in the same folder** — one for work, one for the exam, one you are not sure about — and switch with *Open another calculator…*. They share the ROM, so the second one costs almost nothing.

## Your keyboard, your keys

The whole physical keyboard is remappable, and the way you find out what a key does is to **hover over it**: the calculator tells you which keys on your keyboard press it.

- **Right-click any key** on the face to change it. Press the key you want. Done.
- Two names on one calculator key means two keys on your keyboard reach it, which is usually what you want — `Enter` and the keypad `Enter` both press `ENTER`.
- On a phone there is no right button, so *Customize keyboard…* in the menu turns the same thing on for a plain tap.

Nothing is baked in. The whole map lives in a small text file **beside your ROM**, in the same folder as the calculator's memory — so your bindings travel with the calculator instead of staying behind on one machine.

## As fast as you want it, including "not"

A real HP 48 runs at about 2 MHz in 1990 silicon, and there is a switch for that: **Slow down to real calculator speed** makes it as slow as the one in the drawer, deliberately, because sometimes the point is the calculator you remember. Otherwise a slider takes it up to as fast as your machine will go, which is a great deal faster than 1990 — useful the first time you run a program that used to take a coffee break.

## Getting things in and out

- **Copy and paste** with the rest of your computer, through the system clipboard.
- **Import and export HP 48 object files** — the `HPHP48-` format — in both directions, which is how a program moves to a real 48, to Emu48, to Droid48, or back. Checked byte-for-byte against Emu48 with a third-party library as the control: of 1206 bytes, 1205 came back identical, and the one that differs is the byte that *names the machine that wrote the file*, which is supposed to.
- **Save memory now** whenever you want it, and automatically when the app goes to the background.

## Small, and stays out of the way

Under 20 MB installed, one file per platform, no runtime to install first. The window resizes freely and keeps the calculator's proportions; you can make it a thin strip beside your work or fill the screen with it. Text sizes are adjustable, because a calculator you keep open all day is a calculator you should be able to read.

## Installing it

One download per machine, no runtime to fetch first, and no account to make.

### Windows

1. Download **`Agape48-0.9.1-windows-x64-setup.exe`**.
2. **Windows will stop you**: *"Windows protected your PC"*. Click **More info**, then **Run anyway**. That message is not a virus warning — it is Windows saying the installer carries no code-signing certificate, which is a purchase rather than a build step. Nothing in the download is packed or obfuscated, and every byte of source that went into it is in this repository.
3. **Next**, **Next**, **Install**. It goes into `Program Files` and adds a Start-menu entry.
4. Start it from the Start menu. It uninstalls like any other program, from *Apps & features*.

### Linux

No root, nothing in `/opt`, and nothing to add to your package manager:

```sh
tar xzf Agape48-0.9.1-linux-x86_64.tar.gz
./Agape48-0.9.1-linux-x86_64/install.sh
```

There is no `chmod` step: `tar` keeps the executable bit, so `install.sh` just runs. (If you unpacked with a graphical archiver that dropped permissions, `sh install.sh` works anyway.)

Everything lands under `~/.local/share/agape48`, the menu entry turns up under **Education**, and typing `agape48` runs it if `~/.local/bin` is on your `PATH`. The Qt runtime travels inside the tarball, so there is no distribution package to chase and nothing to install first — which also means it cannot break when your distribution moves to the next Qt. To remove it: `~/.local/share/agape48/uninstall.sh`, which takes back exactly what it put down and leaves your calculators alone.

The download is 37 MB and unpacks to 98 MB, nearly all of it Qt. It is built for **x86_64** against **glibc 2.39**, so Ubuntu 24.04, Mint 22, Debian 13 or anything newer. On an older distribution it stops at startup with a `GLIBC_2.xx not found` line, which reads like a crash and is not one. Tested on X11.

### Android

1. Download **`Agape48-0.9.1-arm64-v8a.apk`** onto the phone and tap it.
2. **Android will refuse the first time** — *"your phone is not allowed to install unknown apps from this source"* — because the file did not come from the Play Store. Tap **Settings**, allow that one source, and come back. Android asks per *source*, so allowing your browser does not also allow your file manager.
3. Play Protect may then warn about an app from an unrecognised developer. The **Install anyway** button is behind *More details*.
4. Tap **Open**. On a phone the calculator fills the screen, the way Droid48 does.

Both of those warnings are about *where the file came from*, not about what is inside it — and the second one is unavoidable for any app that is not distributed through the Play Store.

Android **8.0 or newer**, and **arm64-v8a**, which is every phone sold since about 2016. A 32-bit phone, or an emulator image built for x86_64, will refuse to install it.

## Then give it a ROM, and this is the only fiddly step

The one thing not in the download is the **ROM image** — the calculator's own firmware, the code HP burned into the real machine. Agape48 does not include it, because it is HP's code and not ours.

It is, however, a free download, and that is not a wink: **HP gave permission for these ROMs to be downloaded in mid-2000**, and they have been on hpcalc.org ever since. That is the archive the whole HP hobby uses.

### Getting one

1. Go to **<https://www.hpcalc.org/hp48/pc/emulators/>**.
2. That page is mostly emulators, not ROMs, and the ROM images are a long way down it. Do not scroll — press **Ctrl-F** (**⌘-F** on a Mac) and search the page for **`HP 48GX Revision`**. That lands you on them.
3. There are eleven, and any of them works. If you want the one to stop thinking about, take **`gxrom-r.zip`** — the last revision of the 48GX, which is the calculator this skin is a photograph of. 314 KB.
4. **Unzip it, and do that before you go looking for the file.** Inside is a single file called **`gxrom-r`** with no extension, 524,288 bytes. That file *is* the ROM: nothing to convert, nothing to unpack further. A file still sitting inside the `.zip` is invisible to Agape48 and to every file dialog, which is the likeliest reason for "I downloaded it and the program cannot see it".
5. If you cannot find the unzipped file afterwards, **sort your Downloads folder by date and look at the oldest thing in it.** These ROMs carry their original timestamps from the early 2000s, so the newest file you have is the one that looks twenty-five years old.

The others, if you are curious rather than in a hurry: `gxrom-k` to `gxrom-r` are 48GX revisions K, L, M, P and R, and `sxrom-a` to `sxrom-j` are the older 48SX. Later is generally better — R fixed bugs that K had — but a 48SX ROM turns Agape48 into a 48SX, which is the point of having the choice.

### Putting it where the calculator looks

Either way round works, and neither needs the other:

**The sure way — rename and drop.** Rename the file to exactly **`rom`**, with no extension, and put it in the calculator's own folder:

| | |
|---|---|
| Windows | `%LOCALAPPDATA%\Agape48\Agape48` |
| Linux | `~/.local/share/Agape48/Agape48` |
| Android | `Android/media/br.gbmaizol.agape48/Agape48 calculators` |

On Android that is much the easier of the two routes: the folder is visible in any file manager and over a USB cable and needs no permission at all, so you plug the phone into a computer and drop the file in.

**The quick way — point the app at it.** Start Agape48 and click the underlined **48GX** in the top right corner of the calculator — that is the menu, and there is no other way in. Then **Settings → HP 48 ROM**, and choose the file where it already sits. The calculator starts the moment you press Open, and the path is remembered, so you never do it again.

Two wrinkles, both about the file dialog. It cannot look inside a `.zip`, so unzip first. And it lists ROM-shaped names, which includes `gxrom-r` and `sxrom-a` — but if you renamed the file to something else and cannot see it, switch the dialog's filter to **All files**.

### When to do it

Whenever you like. Install first and start it, and if there is no ROM yet it says so and **prints the exact folder it looked in** — so the honest order is: install, start it once, read the folder off the screen, drop the file in, start it again.

### The first screen, so it does not worry you

With a fresh ROM the calculator asks **`Try To Recover Memory?`**, because its memory has never been written. That is the real HP 48 asking, exactly as a new one would. Press the **rightmost of the six blank keys** along the top — that is **NO** — and you have a clean calculator.

Where that folder lives is yours to move at any time, which is the same setting that lets one calculator sit in Dropbox and be opened from either machine.

## Free software

Agape48 is free software under the **GNU General Public Licence, version 3**, and it has to be: it stands on the `x48` Saturn core written by Eddie C. Dost and the years of work that Droid48 put into it, both under the GPL. Dost's files say "version 2, or any later version", and version 3 is the later one this project takes — the version that also defends you against patent claims and against anyone shipping this on a device you are not allowed to change. So the source is here, the whole of it, and anything you build on it stays free the same way. The full text is in `LICENSE`.

## For programmers

`Readme_Programmers.md` — the build, the layout, the size budget, the seam between the Qt frontend and the C core, and the one unusual rule: **everything written by hand in this project is in Esperanto.** The comments, the pull requests that get merged, the bug reports, the issues. If you do not read it yet, that page makes the case for two months of evenings.
