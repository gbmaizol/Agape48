# Agape48

An HP 48 emulator: the `x48` Saturn core, a Qt 6 / QML frontend, one binary per platform, as small as it will go.

"HP" spoken in Brazilian Portuguese is *agá-pê*, which is the Greek ἀγάπη. The calculator that people are unreasonably fond of, named after the word for it.

Last updated: 2026aug26-00h05

## Layout

```
Agape48/
├── CMakeLists.txt              size-first build; enforces the lean-module rule
├── cmake/Agape48Size.cmake     agape48::size - every size flag, probed not assumed
├── src/
│   ├── main.cpp
│   ├── core/                   the C side
│   │   ├── x48_shim.h          THE seam: ~16 functions, all the C++ ever sees
│   │   ├── x48_shim.c          adapter to the vendored fork (stubs for now)
│   │   ├── VENDORING.md        which x48 to vendor and how
│   │   └── x48/                (submodule, not committed)
│   └── bridge/                 the C++ side
│       ├── Agape48Engine.*     QML facade: tick, keys, state, clipboard
│       ├── LcdItem.*           QQuickItem drawing the pixel buffer
│       ├── StateFileManager.*  bring-your-own-sync: paths and SAF
│       ├── SkinModel.*         skin + layout, JSON and KML dialects
│       └── KmlParser.*         Emu48 Keypad Mapping Language
├── qml/
│   ├── Main.qml                window, error banner, feedback wiring
│   ├── Calculator.qml          face image + LCD + annunciators, one scaled unit
│   ├── Keypad.qml              MultiPointTouchArea hit-testing (ON+A+F works)
│   └── SettingsSheet.qml       raw-QtQuick settings, no Qt Quick Controls
├── assets/skins/default/
│   ├── face.webp               placeholder; replace with the photograph
│   └── layout.json             schema "agape48.skin/1", 49 keys
└── platform/android/
    ├── src/dk/geeak/agape48/SafBridge.java
    ├── build.gradle            one ABI, R8 on
    └── proguard-rules.pro
```

## The seam

Everything above `x48_shim.c` is Qt; everything below is C from the 1990s. The two never meet. `x48`, `x48ng` and Droid48 each expose a different `saturn` struct and a different main loop, so vendoring a different upstream means rewriting one 200-line C file and nothing else. `src/core/VENDORING.md` has the picking guide - the short version is: start from `x48ng`, because it already has `step_instruction()` factored out of the X11 event pump, and that is exactly the shape `x48_run_slice()` needs.

## Building

```
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.0/gcc_64
cmake --build build -j
```

`MinSizeRel` is the default; you have to ask for anything else. Configure prints every size knob it resolved.

Android:

```
cmake -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DQT_HOST_PATH=/path/to/Qt/6.8.0/gcc_64 \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.0/android_arm64_v8a \
  -DANDROID_ABI=arm64-v8a
cmake --build build-android --target apk
```

## Size budget

Ordered by how much each one actually saves.

| Lever | Where | Rough effect |
| --- | --- | --- |
| Build a static Qt with features off | Qt's own `configure` | by far the biggest; see below |
| `MinSizeRel` + `-Os` | default | baseline |
| LTO | `AGAPE48_LTO=ON` | 10-20% on a Qt Quick binary |
| `--gc-sections` + `-ffunction-sections` | `agape48::size` | 5-15% |
| Plugin exclusion | `qt_import_plugins(EXCLUDE_BY_TYPE …)` | large in static builds, nil in shared |
| ICF (`--icf=all`, lld/gold) | `AGAPE48_ICF=ON` | 3-6%, template-heavy code folds well |
| No QML cachegen | `AGAPE48_QML_CACHEGEN=OFF` | small; costs startup time |
| `-fvisibility=hidden` | `agape48::size` | smaller dynsym, better LTO |
| One ABI on Android | `abiFilters 'arm64-v8a'` | divides the .so payload by the ABI count |

The single largest lever is not in this repo. A stock Qt binary is built for everything; a Qt configured for this app is a fraction of it:

```
./configure -static -release -optimize-size -ltcg \
  -no-feature-networkproxy -no-feature-dnslookup -no-feature-sql \
  -no-feature-printsupport -no-feature-testlib -no-feature-concurrent \
  -no-feature-itemmodel -no-feature-textodfwriter \
  -skip qtnetworkauth,qtwebengine,qtmultimedia,qtcharts,qt3d,qtquick3d \
  -submodules qtbase,qtdeclarative,qtimageformats \
  -qt-libpng -no-libjpeg -no-feature-printer
```

Static linking Qt under the LGPL obliges you to let a recipient relink the application against a modified Qt - in practice, publish the object files or the full build recipe. `x48` is GPL, so Agape48 is GPL too and the source has to ship with the binaries either way. Droid48 and droid48sx both settled on GPLv3. Worth settling before the first release, not after.

The ROM can be bundled. HP's ACO allowed non-commercial use of the HP 48 ROMs in autumn 2000, which is the basis Droid48 has shipped both the 48G and 48S ROM on for years. That holds only while the app is free.

## WebP is not free

`QImage` cannot read WebP out of the box. The `qwebp` plugin lives in **qtimageformats**, which is a separate Qt module from Core/Gui/Qml/Quick. Three ways out:

1. Link the plugin (`AGAPE48_WEBP_PLUGIN=ON`, the default). Costs the plugin plus libwebp, on the order of 200-400 KB static.
2. Link libwebp's decoder only (`libwebpdecoder`) and call `WebPDecodeRGBA` yourself in a 30-line `QQuickImageProvider`. Smaller than the plugin, and it drops the encoder you will never use.
3. Ship the face as PNG instead. Qt has PNG built in, so the module cost is zero - but a photorealistic 480×900 face is roughly 3-5× the bytes of the same image as lossy WebP, so this only wins if the plugin is the thing you are trying to avoid.

Option 2 is the smallest total and it is not much work. Option 1 is the default because it is one CMake line.

## Sound and haptics

`QSoundEffect` is in Qt Multimedia, so "use QSoundEffect, not Multimedia" cannot both hold. Qt Multimedia is a megabyte-class dependency plus platform backends, for a square-wave beep. The lean path is a ~60-line `Feedback` singleton per platform: Android `AudioTrack` (or `ToneGenerator`) and `Vibrator`/`VibrationEffect` through `QJniObject`, Linux a raw ALSA square wave or nothing at all, Windows `Beep()` on a worker thread. Haptics on Android need `<uses-permission android:name="android.permission.VIBRATE"/>`, which is a normal permission and needs no prompt. Not written yet - `qml/Main.qml` has the call sites stubbed and named.

## Sync conflicts

Byte-identical state files across three OSes make syncing trivial and conflict resolution impossible. Two devices that both open the calculator between syncs produce two divergent RAM images, and a RAM image cannot be merged - it is a heap with pointers into itself.

What Agape48 does about it: save on every background/suspend, fingerprint what it wrote, and on resume compare the fingerprint against what is on disk. A mismatch means another device wrote it, and `conflictDetected` fires. What it deliberately does *not* do is silently pick a winner. Your sync client already keeps the loser as a conflicted copy; the app's job is to say so, not to guess.

The honest version of this feature is "one calculator, several machines, one at a time" - the same contract a KeePass database has. Say that in the UI and it is a feature; leave it unsaid and it is a bug report about lost variables.

## Deliberate non-dependencies

Qt Quick Controls is not linked. It would add roughly a megabyte of styles to a static binary, and its widgets look wrong over a photograph of a calculator. The cost lands in `SettingsSheet.qml`: a text field is a `TextInput` inside a `Rectangle`, and there is no `FolderDialog`. Folder picking is a typed path plus a `DropArea` on desktop, and SAF on Android. `QtQuick.Dialogs` would give you a native folder dialog, but its fallback implementation pulls Controls back in, so it is Controls with extra steps.

`CMakeLists.txt` fails the configure step if any forbidden module is linked directly, so this stays true by accident rather than by discipline.
