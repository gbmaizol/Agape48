# Agape48

[English](README.md) · [Dansk](README.da.md) · **Deutsch** · [Español](README.es.md) · [Esperanto](README.eo.md) · [Français](README.fr.md) · [Português](README.pt-BR.md)

**Der HP 48, den du längst liebst, auf dem Computer und dem Telefon, die du wirklich benutzt — und er nimmt deinen Rechner mit.**

„HP“ auf brasilianisches Portugiesisch ausgesprochen wird zu *agá-pê*, und das ist das griechische ἀγάπη: Liebe. Agape48 ist danach benannt, wie die Leute über diesen Rechner reden.

---

## Wenn du noch nie einen HP benutzt hast, lies zuerst diesen Teil

Fast jeder Taschenrechner, den du je in der Hand hattest, arbeitet so:

```
( 3 + 4 ) × 5 =
```

Du musstest zwei Klammern tippen, um zu sagen, was du meintest, und erst ganz am Ende hast du erfahren, ob es das war. Vertipp dich bei einer davon, und das Ergebnis ist falsch, ohne ein Wort.

Ein HP 48 arbeitet so:

```
3 ENTER 4 + 5 ×
```

Sag, was du hast, und sag dann, was damit geschehen soll. Mehr ist UPN nicht, die umgekehrte polnische Notation, die auch RPN heißt. **Du tippst nie wieder eine Klammer**, und der Rechner zeigt dir jedes Zwischenergebnis in dem Moment, in dem es entsteht — nach `4 +` liegt die `7` sichtbar da, also zeigt sich ein Fehler in dem Augenblick, in dem du ihn machst, und nicht am Schluss.

Es liest sich ungefähr zwanzig Minuten lang seltsam und danach liest es sich wie Rechnen. Drei Dinge passieren dann:

- **Du hörst auf, Tastendrücke zu zählen** — UPN braucht meistens weniger, und nie mehr.
- **Du hörst auf, neu anzufangen.** Alles, was du getippt hast, bleibt auf einem Stapel liegen, sichtbar, vor dir. Die letzte Zahl verhauen? Das ist eine Taste, um sie wegzuwerfen, kein Neustart.
- **Du hörst auf, einer einzigen Zeile zu trauen.** Eine lange Rechnung auf einem normalen Taschenrechner ist eine Zahl und eine Hoffnung. Hier sind es vier Zahlen, die du anschauen kannst.

Wenn du die Kurzfassung willst: tippe die Zahlen, drücke `ENTER` dazwischen, und drücke dann die Rechenart. Das reicht, um am ersten Tag echte Arbeit zu machen.

## Warum ausgerechnet dieser

HP-48-Emulatoren gibt es seit dreißig Jahren, und sie sind gut — Agape48 steht auf dem Kern von einem davon. Was es nie gab, ist *einer* von ihnen, der sich auf deinem Laptop und deinem Telefon gleich verhält und den Rechner zwischen beiden hin und her reicht.

| | Windows | Linux | Android | liest HP-48-Objektdateien | schreibt sie |
| --- | :---: | :---: | :---: | :---: | :---: |
| **Agape48** | **ja** | **ja** | **ja** | **ja** | **ja** |
| Emu48 | ja | — | — | ja | ja |
| Droid48 | — | — | ja | ja | — |
| x48 | — | ja | — | — | — |

Dieses letzte Spaltenpaar ist das, worauf es in der Praxis ankommt: eine HP-48-Objektdatei ist der Weg, auf dem ein Programm oder eine Matrix zwischen all diesen und einem echten Rechner wandert, und bis jetzt konnte kein einzelnes Programm sie überall lesen *und* schreiben. Auf Android konnte gar nichts beides.

## Dein Rechner, nicht eine Kopie davon

Der Speicher des Rechners ist **ein Ordner, den du aussuchst**. Leg diesen Ordner in Dropbox, OneDrive, Syncthing, auf einen USB-Stick — irgendetwas, das Dateien abgleicht — und der Rechner, den du am Laptop benutzt hast, ist der Rechner, der auf dem Telefon aufgeht, mit deinen Variablen, deinen Programmen und deinem Stapel genau da, wo du sie gelassen hast.

Zwei ehrliche Einzelheiten, denn das ist der Teil, den andere Emulatoren dich selbst herausfinden lassen:

- **Einer zur Zeit.** Der Speicher eines Rechners ist ein Haufen mit Zeigern in sich selbst; zwei Geräte, die ihn gleichzeitig bearbeiten, kann niemand zusammenführen. Also legt Agape48 eine Marke in den Ordner, die sagt, welche Maschine ihn offen hat, meldet sich, wenn eine andere ihn hat, und **sagt es, statt stillschweigend einen Sieger zu küren**. Das ist derselbe Handel, den dir eine Passwortdatenbank anbietet, und aus demselben Grund.
- **Es sind wirklich nur Dateien.** Kein Konto, keine Wolke von uns, nichts, das nach Hause telefoniert. Dein Sync-Programm war darin schon gut; Agape48 versucht nicht, es zu ersetzen.

Du kannst **mehrere Rechner im selben Ordner** halten — einen für die Arbeit, einen für die Prüfung, einen, bei dem du dir nicht sicher bist — und mit *Open another calculator…* wechseln. Sie teilen sich die ROM, also kostet der zweite fast nichts.

## Deine Tastatur, deine Tasten

Die ganze physische Tastatur lässt sich neu belegen, und der Weg herauszufinden, was eine Taste tut, ist, **mit der Maus darüber zu fahren**: der Rechner sagt dir, welche Tasten deiner Tastatur sie drücken.

- **Rechtsklick auf eine beliebige Taste** der Frontplatte, um sie zu ändern. Drück die Taste, die du haben willst. Fertig.
- Zwei Namen auf einer Rechnertaste heißt, dass zwei Tasten deiner Tastatur sie erreichen, was meistens genau das ist, was du willst — `Enter` und das `Enter` des Ziffernblocks drücken beide `ENTER`.
- Auf einem Telefon gibt es keine rechte Taste, also schaltet *Customize keyboard…* im Menü dasselbe für einen einfachen Tipp frei.

Nichts ist fest eingebacken. Die ganze Belegung wohnt in einer kleinen Textdatei **neben deiner ROM**, im selben Ordner wie der Speicher des Rechners — deine Zuordnungen reisen also mit dem Rechner, statt auf einer Maschine zurückzubleiben.

## So schnell, wie du willst, auch „gar nicht“

Ein echter HP 48 läuft mit etwa 2 MHz in Silizium von 1990, und dafür gibt es einen Schalter: **Slow down to real calculator speed** macht ihn so langsam wie den in der Schublade, mit Absicht, denn manchmal ist es gerade der Rechner, an den man sich erinnert. Sonst zieht ein Regler ihn hoch bis an das, was deine Maschine hergibt, und das ist erheblich schneller als 1990 — nützlich beim ersten Mal, wenn du ein Programm laufen lässt, das früher eine Kaffeepause dauerte.

## Sachen hinein- und herausbekommen

- **Kopieren und Einfügen** mit dem Rest deines Computers, über die Zwischenablage des Systems.
- **HP-48-Objektdateien importieren und exportieren** — das Format `HPHP48-` — in beide Richtungen, und das ist der Weg, auf dem ein Programm zu einem echten 48, zu Emu48, zu Droid48 oder zurück kommt. Byte für Byte gegen Emu48 geprüft, mit einer fremden Bibliothek als Kontrolle: von 1206 Bytes kamen 1205 identisch zurück, und das eine, das abweicht, ist das Byte, das *die schreibende Maschine benennt*, was es auch soll.
- **Save memory now**, wann immer du willst, und automatisch, wenn die App in den Hintergrund geht.

## Eigenständig, und nicht im Weg

Eine Datei pro Plattform, mit einer eigenen Kopie von Qt darin, sodass keine Laufzeitumgebung vorher installiert werden muss. Das Fenster lässt sich frei skalieren und hält die Proportionen des Rechners; du kannst einen schmalen Streifen neben deiner Arbeit daraus machen oder den Bildschirm damit füllen. Die Textgrößen sind einstellbar, denn ein Rechner, den du den ganzen Tag offen hast, ist ein Rechner, den du lesen können solltest.

## Installieren

Ein Download pro Maschine, keine Laufzeitumgebung vorher zu holen, und kein Konto anzulegen.

**Alle drei liegen auf der [Releases-Seite](https://github.com/gbmaizol/Agape48/releases/latest)**, mit einem sha256 neben jedem, falls du prüfen willst, was du bekommen hast.

Die unten zitierten Warnungen erscheinen in der Sprache deines Systems. Sie stehen hier auf Englisch, weil Windows und Android sie auf einer englischen Installation so schreiben.

### Windows

1. Lade **[`Agape48-0.9.2-windows-x64-setup.exe`](https://github.com/gbmaizol/Agape48/releases/latest)** (17,3 MB) herunter.
2. **Windows hält dich auf**: *„Windows protected your PC“*. Klick **More info**, dann **Run anyway**. Diese Meldung ist keine Virenwarnung — es ist Windows, das sagt, dass das Installationsprogramm kein Code-Signing-Zertifikat trägt, und das ist ein Kauf und kein Bauschritt. Nichts am Download ist gepackt oder verschleiert, und jedes Byte Quelltext, das hineingegangen ist, liegt in diesem Repository.
3. **Next**, **Next**, **Install**. Es landet in `Program Files` und legt einen Eintrag im Startmenü an.
4. Starte es aus dem Startmenü. Es deinstalliert sich wie jedes andere Programm, über *Apps & features*.

### Linux

Kein root, nichts in `/opt`, und nichts, was der Paketverwaltung hinzugefügt werden muss. Nimm **[`Agape48-0.9.2-linux-x86_64.tar.gz`](https://github.com/gbmaizol/Agape48/releases/latest)**, dann:

```sh
tar xzf Agape48-0.9.2-linux-x86_64.tar.gz
./Agape48-0.9.2-linux-x86_64/install.sh
```

Es gibt keinen `chmod`-Schritt: `tar` behält das Ausführungsbit, also läuft `install.sh` einfach. (Wenn du mit einem grafischen Archivprogramm entpackt hast, das die Rechte verloren hat, funktioniert `sh install.sh` trotzdem.)

Alles landet unter `~/.local/share/agape48`, der Menüeintrag taucht unter **Education** auf, und `agape48` zu tippen startet ihn, wenn `~/.local/bin` in deinem `PATH` liegt. Die Qt-Laufzeitumgebung reist im Tarball mit, es gibt also kein Distributionspaket zu jagen und nichts vorher zu installieren — was auch heißt, dass nichts kaputtgeht, wenn deine Distribution auf das nächste Qt weitergeht. Zum Entfernen: `~/.local/share/agape48/uninstall.sh`, das genau das zurücknimmt, was es hingelegt hat, und deine Rechner in Ruhe lässt.

Der Download ist 35,9 MB groß und entpackt sich auf 96 MB, fast alles davon Qt. Gebaut für **x86_64** gegen **glibc 2.39**, also Ubuntu 24.04, Mint 22, Debian 13 oder irgendetwas Neueres. Auf einer älteren Distribution bleibt er beim Start mit einer Zeile `GLIBC_2.xx not found` stehen, was wie ein Absturz aussieht und keiner ist. Getestet unter X11.

### Android

1. Lade **[`Agape48-0.9.2-arm64-v8a.apk`](https://github.com/gbmaizol/Agape48/releases/latest)** (47,4 MB) auf das Telefon und tippe sie an.
2. **Android weigert sich beim ersten Mal** — *„your phone is not allowed to install unknown apps from this source“* — weil die Datei nicht aus dem Play Store kam. Tippe **Settings**, erlaube genau diese eine Quelle, und komm zurück. Android fragt pro *Quelle*, deinen Browser zu erlauben erlaubt also nicht auch deinen Dateimanager.
3. Play Protect warnt danach vielleicht vor einer App von einem unbekannten Entwickler. Der Knopf **Install anyway** steckt hinter *More details*.
4. Tippe **Open**. Auf einem Telefon füllt der Rechner den Bildschirm, so wie Droid48 es tut.

Beide Warnungen drehen sich darum, *woher die Datei kam*, nicht darum, was darin ist — und die zweite ist für jede App unvermeidlich, die nicht über den Play Store verteilt wird.

Android **8.0 oder neuer**, und **arm64-v8a**, also jedes Telefon, das seit etwa 2016 verkauft wurde. Ein 32-Bit-Telefon oder ein für x86_64 gebautes Emulator-Abbild wird die Installation verweigern.

## Dann gib ihm eine ROM, und das ist der einzige fummelige Schritt

Das eine, was nicht im Download steckt, ist das **ROM-Abbild** — die Firmware des Rechners selbst, der Code, den HP in die echte Maschine gebrannt hat. Agape48 enthält ihn nicht, denn es ist HPs Code und nicht unserer.

Er ist allerdings ein kostenloser Download, und das ist kein Augenzwinkern: **HP hat Mitte 2000 die Erlaubnis erteilt, diese ROMs herunterzuladen**, und seitdem liegen sie auf hpcalc.org. Das ist das Archiv, das das ganze HP-Hobby benutzt.

### Eine bekommen

1. Geh auf **<https://www.hpcalc.org/hp48/pc/emulators/>**.
2. Diese Seite besteht größtenteils aus Emulatoren, nicht aus ROMs, und die Abbilder liegen sehr weit unten. Scroll nicht — drück **Strg-F** (**⌘-F** auf einem Mac) und such auf der Seite nach **`HP 48GX Revision`**. Das setzt dich direkt darauf ab.
3. Es sind elf, und jede davon funktioniert. Wenn du die willst, über die du nicht mehr nachdenken musst, nimm **`gxrom-r.zip`** — die letzte Revision des 48GX, und das ist der Rechner, nach dem diese Frontplatte gezeichnet ist. 314 KB.
4. **Pack sie aus, und zwar bevor du nach der Datei suchst.** Darin liegt eine einzige Datei namens **`gxrom-r`** ohne Endung, 524.288 Bytes groß. Diese Datei *ist* die ROM: nichts umzuwandeln, nichts weiter auszupacken. Eine Datei, die noch im `.zip` steckt, ist für Agape48 und für jeden Dateidialog unsichtbar, und das ist der wahrscheinlichste Grund für „ich hab sie geladen und das Programm findet sie nicht“.
5. Wenn du die entpackte Datei danach nicht wiederfindest, **sortier deinen Downloads-Ordner nach Datum und schau dir das Älteste darin an.** Diese ROMs tragen ihre ursprünglichen Zeitstempel aus den frühen 2000ern, die neueste Datei, die du hast, ist also die, die fünfundzwanzig Jahre alt aussieht.

Die übrigen, falls du neugierig statt in Eile bist: `gxrom-k` bis `gxrom-r` sind die 48GX-Revisionen K, L, M, P und R, und `sxrom-a` bis `sxrom-j` sind der ältere 48SX. Später ist in der Regel besser — R hat Fehler behoben, die K hatte — aber eine 48SX-ROM macht aus Agape48 einen 48SX, und genau darum lohnt sich die Wahl.

### Sie dahin legen, wo der Rechner hinschaut

Beide Wege funktionieren, und keiner braucht den anderen:

**Der sichere Weg — umbenennen und ablegen.** Benenne die Datei in genau **`rom`** um, ohne Endung, und leg sie in den Ordner des Rechners selbst:

| | |
|---|---|
| Windows | `%LOCALAPPDATA%\Agape48\Agape48` |
| Linux | `~/.local/share/Agape48/Agape48` |
| Android | `Android/media/br.gbmaizol.agape48/Agape48 calculators` |

Auf Android ist das mit Abstand der leichtere der beiden Wege: der Ordner ist in jedem Dateimanager und über ein USB-Kabel sichtbar und braucht überhaupt keine Berechtigung, du hängst das Telefon also an einen Computer und legst die Datei hinein.

**Der schnelle Weg — zeig der App, wo sie liegt.** Starte Agape48 und klick auf das unterstrichene **48GX** in der oberen rechten Ecke des Rechners — das ist das Menü, und einen anderen Eingang gibt es nicht. Dann **Settings → HP 48 ROM**, und wähl die Datei da aus, wo sie schon liegt. Der Rechner startet in dem Moment, in dem du Open drückst, und der Pfad wird gemerkt, du machst das also nie wieder.

Zwei Falten, beide zum Dateidialog. Er kann nicht in ein `.zip` hineinschauen, also vorher auspacken. Und er listet ROM-förmige Namen auf, wozu `gxrom-r` und `sxrom-a` gehören — aber wenn du die Datei anders benannt hast und sie nicht siehst, stell den Filter des Dialogs auf **All files**.

### Wann du das machst

Wann immer du magst. Installier zuerst und starte ihn, und wenn noch keine ROM da ist, sagt er das und **schreibt den genauen Ordner hin, in den er geschaut hat** — die ehrliche Reihenfolge ist also: installieren, einmal starten, den Ordner vom Bildschirm ablesen, die Datei hineinlegen, wieder starten.

### Der erste Bildschirm, damit er dich nicht beunruhigt

Mit einer frischen ROM fragt der Rechner **`Try To Recover Memory?`**, weil sein Speicher nie beschrieben wurde. Das ist der echte HP 48, der fragt, genau wie ein neuer es täte. Drück die **rechteste der sechs leeren Tasten** oben — das ist **NO** — und du hast einen sauberen Rechner.

Wo dieser Ordner wohnt, gehört dir und lässt sich jederzeit verschieben, und es ist dieselbe Einstellung, die einen Rechner in Dropbox wohnen und von beiden Maschinen aus öffnen lässt.

## Freie Software

Agape48 ist freie Software unter der **GNU General Public License, Version 3**, und muss es sein: es steht auf dem Saturn-Kern `x48`, geschrieben von Eddie C. Dost, und auf den Jahren Arbeit, die Droid48 hineingesteckt hat, beide unter der GPL. Dosts Dateien sagen „Version 2, oder jede spätere Version“, und Version 3 ist die spätere, die dieses Projekt nimmt — die Version, die dich auch gegen Patentansprüche verteidigt und gegen jeden, der dies auf einem Gerät ausliefert, das du nicht verändern darfst. Der Quelltext liegt also hier, der ganze, und alles, was darauf aufbaut, bleibt auf dieselbe Weise frei. Der vollständige Text steht in `LICENSE`.

## Für Programmierer

`Readme_Programmers.md` — der Bau, der Aufbau, das Größenbudget, die Naht zwischen der Qt-Oberfläche und dem C-Kern, und die eine ungewöhnliche Regel: **alles von Hand Geschriebene in diesem Projekt ist auf Esperanto.** Die Kommentare, die Pull Requests, die gemergt werden, die Fehlermeldungen, die Issues. Wenn du es noch nicht liest, macht jene Seite den Fall für zwei Monate Abende auf.
