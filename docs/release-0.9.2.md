# Agape48 0.9.2

Two things worth a release: a calculator that stops looking absurd on a tablet, and a missing arrow on the keyboard.

## The tablet

Agape48 used to fill a tablet's screen the way it fills a phone's, and that was wrong twice over.

**It was distorted.** On Android the two axes get their own scale, which is what Droid48 does and what makes the keypad reach the bottom of a phone. A phone screen is *narrower* than the calculator face, so the stretch makes keys taller — which is right. A 13-inch tablet is *wider* than the face, and the same rule made every key **19% too wide**. That stops now at Android's own tablet line, `sw600dp`. Phones behave exactly as before, down the same code path.

**And it was absurdly large.** A calculator 193 mm across, with keys the size of a matchbox. On a screen bigger than the machine itself, the machine now stops growing at **91 mm — the width of a real 48GX** — and sits in the middle of the screen. On a 13-inch tablet that is under a quarter of the area.

Two different measurements decide those, because they answer different questions. *Whether* the screen is big is decided in density-independent pixels, because that is what Android itself uses and it is never wrong. *How big to draw* is decided in millimetres, because "absurdly large" is a question about a hand — and a tablet reports its true physical size accurately enough to trust: 315.9 × 314.4 real dots per inch, giving a 13.19-inch diagonal on a device sold as 13.2.

The window also now declares itself resizable and names the shape it wants, so when a tablet puts Agape48 in a floating window it opens as a calculator instead of being dragged into one.

## The arrow that was missing

The left-shift legend on `EVAL` is **`→NUM`** on a real HP 48GX. This face said `NUM`.

That is a small thing and an instructive one, because **the face is not a photograph**. Every key, gradient and legend is drawn from the real machine's measurements. It is made to read as a photograph, and it isn't one — which is exactly why a detail could be missing at all. On a photograph it could not have happened.

So the rest of the keyboard was checked rather than eyeballed: all 49 keys diffed against the emulator core's own button table. Everything else agrees. The one near-miss is worth recording — the core files nine application labels (`SOLVE`, `PLOT`, `SYMBOLIC`, `TIME`, `STAT`, `UNITS`, `I/O`, `LIBRARY`, `EQ LIB`) in a way that would make them right-shift green, but the flag that would mean is declared and never read. Measured on the photograph of a real keyboard instead: that ink sits at hue 203–206, against the left-shift key at 213 and the right-shift at 174. Left-shift. They were already right.

**About Agape48 stopped saying the face was a photograph**, along with five comments that said the same. It now says what is true, which is the better claim anyway: the lettering stays sharp at every window size *because* it is drawn rather than photographed.

The icon is still a photograph — of a real 48GX, taken for this project — and that has not changed.

## Everything else is 0.9.1

Same ROM handling, same file formats, same calculators. Android upgrades in place; your calculators stay where they are.

**No ROM is included** — it is HP's code, it is a free download from hpcalc.org, and the [README](https://github.com/gbmaizol/Agape48/blob/main/README.md#then-give-it-a-rom-and-this-is-the-only-fiddly-step) names the exact file and what to do with it.

## What is still missing

No ROM ships with it; the Android build has never been measured for speed; and on a tablet Agape48 still cannot put *itself* into a floating window — Android does not let an app do that, and the route that would is a rebuild of the Android side rather than a setting. Those are what `1.0.0` waits for.

## Licence

GPLv3, in [LICENSE](https://github.com/gbmaizol/Agape48/blob/main/LICENSE). Agape48 stands on the `x48` Saturn core by Eddie C. Dost and on the years of work Droid48 put into it, both under the GPL.
