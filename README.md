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

## Running it

You need an **HP 48 ROM image** — the calculator's own firmware. Agape48 does not include one: it is HP's code, not ours, and a program that ships it is making a promise it has no paper for. ROM images are the single most-mirrored file in this hobby and are a search away; put the file next to the calculator's memory, name it `rom`, or just point *Settings* at it.

Everything else is one download. Where the calculator's memory lives is printed at startup and is yours to move at any time.

## Free software

Agape48 is free software, and it has to be: it stands on the `x48` Saturn core written by Eddie C. Dost and the years of work that Droid48 put into it, both under the GNU General Public Licence. So the source is here, the whole of it, and anything you build on it stays free the same way. See `LICENSE`.

## For programmers

`Readme_Programmers.md` — the build, the layout, the size budget, the seam between the Qt frontend and the C core, and the one unusual rule: **everything written by hand in this project is in Esperanto.** The comments, the pull requests that get merged, the bug reports, the issues. If you do not read it yet, that page makes the case for two months of evenings.
