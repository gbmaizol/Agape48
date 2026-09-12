# Agape48

[English](README.md) · **Dansk** · [Español](README.es.md) · [Esperanto](README.eo.md) · [Português](README.pt-BR.md)

**Den HP 48 du allerede elsker, på den computer og den telefon du rent faktisk bruger — og den tager din lommeregner med sig.**

«HP» udtalt på brasiliansk portugisisk bliver til *agá-pê*, som er det græske ἀγάπη: kærlighed. Agape48 er opkaldt efter den måde, folk taler om denne lommeregner på.

---

## Hvis du aldrig har brugt en HP, så læs denne del først

Næsten alle lommeregnere, du nogensinde har rørt ved, fungerer sådan her:

```
( 3 + 4 ) × 5 =
```

Du var nødt til at taste to parenteser for at sige, hvad du mente, og du fandt først ud af det til allersidst. Tag fejl af den ene, og svaret er forkert, uden at der bliver sagt noget.

En HP 48 fungerer sådan her:

```
3 ENTER 4 + 5 ×
```

Sig hvad du har, og sig så hvad der skal ske med det. Det er hele RPN. **Du taster aldrig en parentes igen**, og lommeregneren viser dig hvert mellemresultat, mens det sker — efter `4 +` kan du se `7` ligge der, så en fejl er synlig i det øjeblik du laver den, i stedet for til sidst.

Det læses underligt i omkring tyve minutter, og bagefter læses det som regning. Tre ting sker derefter:

- **Du holder op med at tælle tastetryk** — RPN kræver som regel færre, og aldrig flere.
- **Du holder op med at starte forfra.** Alt hvad du har tastet, bliver liggende på en stak, synligt, foran dig. Rodet i det sidste tal? Det er én tast at smide det væk, ikke en frisk start.
- **Du holder op med at stole på en enkelt linje.** En lang udregning på en almindelig lommeregner er ét tal og ét håb. Her er det fire tal, du kan kigge på.

Vil du have den korte version: tast tallene, tryk `ENTER` mellem dem, og tryk så på regnearten. Det er nok til at lave rigtigt arbejde på dag ét.

## Hvorfor lige denne

Der har været HP 48-emulatorer i tredive år, og de er gode — Agape48 er bygget på kernen fra en af dem. Det, der aldrig har været, er *én* af dem, der opfører sig ens på din bærbare og din telefon, og som rækker lommeregneren frem og tilbage mellem dem.

| | Windows | Linux | Android | læser HP 48-objektfiler | skriver dem |
| --- | :---: | :---: | :---: | :---: | :---: |
| **Agape48** | **ja** | **ja** | **ja** | **ja** | **ja** |
| Emu48 | ja | — | — | ja | ja |
| Droid48 | — | — | ja | ja | — |
| x48 | — | ja | — | — | — |

Det sidste par kolonner er det, der betyder noget i praksis: en HP 48-objektfil er måden, hvorpå et program eller en matrix flytter sig mellem alle disse og en rigtig lommeregner, og indtil nu ville intet enkelt program både læse og skrive dem alle steder. På Android ville intet gøre begge dele overhovedet.

## Din lommeregner, ikke en kopi af den

Lommeregnerens hukommelse er **en mappe, du vælger**. Læg den mappe i Dropbox, OneDrive, Syncthing, på en USB-nøgle — hvad som helst der synkroniserer filer — og den lommeregner, du sad med på den bærbare, er den lommeregner, der åbner på telefonen, med dine variabler, dine programmer og din stak præcis hvor du slap.

To ærlige detaljer, for det er den del, andre emulatorer lader dig opdage selv:

- **Én ad gangen.** En lommeregners hukommelse er en dynge med pegepinde ind i sig selv; to enheder, der redigerer den samtidig, kan ingen flette sammen. Derfor lægger Agape48 et mærke i mappen, der siger hvilken maskine der har den åben, siger til når en anden har den, og **siger det i stedet for stiltiende at vælge en vinder**. Det er den samme aftale, en adgangskodedatabase giver dig, og af samme grund.
- **Det er virkelig bare filer.** Ingen konto, ingen sky fra os, intet der ringer hjem. Dit synkroniseringsprogram var allerede godt til det; Agape48 forsøger ikke at erstatte det.

Du kan have **flere lommeregnere i den samme mappe** — en til arbejdet, en til eksamen, en du ikke er sikker på — og skifte med *Open another calculator…*. De deler ROM'en, så den anden koster næsten ingenting.

## Dit tastatur, dine taster

Hele det fysiske tastatur kan mappes om, og måden at finde ud af, hvad en tast gør, er at **holde musen over den**: lommeregneren fortæller dig, hvilke taster på dit tastatur der trykker på den.

- **Højreklik på en hvilken som helst tast** på forpladen for at ændre den. Tryk på den tast, du vil have. Færdig.
- To navne på én lommeregnertast betyder, at to taster på dit tastatur når den, hvilket som regel er det, du vil have — `Enter` og `Enter` på det numeriske tastatur trykker begge på `ENTER`.
- På en telefon er der ingen højre knap, så *Customize keyboard…* i menuen slår det samme til for et almindeligt tryk.

Intet er bagt fast. Hele kortet bor i en lille tekstfil **ved siden af din ROM**, i samme mappe som lommeregnerens hukommelse — så dine tastebindinger rejser med lommeregneren i stedet for at blive tilbage på én maskine.

## Så hurtig du vil have den, også «slet ikke»

En rigtig HP 48 kører ved omkring 2 MHz i 1990-silicium, og der findes en kontakt til det: **Slow down to real calculator speed** gør den lige så langsom som den i skuffen, med vilje, for nogle gange er pointen netop den lommeregner, du husker. Ellers tager en skyder den op til så hurtigt, din maskine kan, hvilket er en hel del hurtigere end 1990 — nyttigt første gang du kører et program, der før tog en kaffepause.

## At få ting ind og ud

- **Kopiér og indsæt** med resten af din computer, gennem systemets udklipsholder.
- **Importér og eksportér HP 48-objektfiler** — formatet `HPHP48-` — begge veje, hvilket er sådan et program flytter sig til en rigtig 48, til Emu48, til Droid48, eller tilbage. Kontrolleret byte for byte mod Emu48 med et tredjepartsbibliotek som kontrol: af 1206 byte kom 1205 identiske tilbage, og den ene, der adskiller sig, er den byte, der *navngiver den maskine, som skrev filen*, hvilket den skal.
- **Save memory now** når du har lyst, og automatisk når appen går i baggrunden.

## Lille, og i vejen for ingen

Under 20 MB installeret, én fil per platform, ingen runtime at installere først. Vinduet kan frit skifte størrelse og holder lommeregnerens proportioner; du kan gøre den til en smal strimmel ved siden af dit arbejde eller fylde skærmen med den. Tekststørrelser kan justeres, for en lommeregner, du har åben hele dagen, er en lommeregner, du bør kunne læse.

## Sådan installerer du den

Ét download per maskine, ingen runtime at hente først, og ingen konto at oprette.

**Alle tre ligger på [udgivelsessiden](https://github.com/gbmaizol/Agape48/releases/latest)**, med en sha256 ved siden af hver, hvis du vil kontrollere, hvad du fik.

Advarselsteksterne, der citeres nedenfor, vises på dit systems sprog. De står her på engelsk, fordi det er sådan Windows og Android skriver dem på en engelsk installation.

### Windows

1. Hent **[`Agape48-0.9.2-windows-x64-setup.exe`](https://github.com/gbmaizol/Agape48/releases/latest)**.
2. **Windows stopper dig**: *«Windows protected your PC»*. Klik **More info**, derefter **Run anyway**. Den besked er ikke en virusadvarsel — det er Windows, der siger, at installationsprogrammet ikke bærer noget kodesigneringscertifikat, hvilket er et køb snarere end et byggetrin. Intet i downloadet er pakket væk eller sløret, og hver eneste byte kildekode, der gik ind i det, ligger i dette repository.
3. **Next**, **Next**, **Install**. Den lander i `Program Files` og tilføjer et punkt i Start-menuen.
4. Start den fra Start-menuen. Den afinstalleres som ethvert andet program, fra *Apps & features*.

### Linux

Ingen root, intet i `/opt`, og intet at tilføje til din pakkehåndtering. Tag **[`Agape48-0.9.2-linux-x86_64.tar.gz`](https://github.com/gbmaizol/Agape48/releases/latest)**, derefter:

```sh
tar xzf Agape48-0.9.2-linux-x86_64.tar.gz
./Agape48-0.9.2-linux-x86_64/install.sh
```

Der er ikke noget `chmod`-trin: `tar` bevarer kørselsbitten, så `install.sh` kører bare. (Hvis du pakkede ud med et grafisk arkivprogram, der tabte rettighederne, virker `sh install.sh` alligevel.)

Alt lander under `~/.local/share/agape48`, menupunktet dukker op under **Education**, og at taste `agape48` kører den, hvis `~/.local/bin` er i din `PATH`. Qt-runtimen rejser inde i tarball'en, så der er ingen distributionspakke at jagte og intet at installere først — hvilket også betyder, at den ikke kan gå i stykker, når din distribution går videre til næste Qt. For at fjerne den: `~/.local/share/agape48/uninstall.sh`, som tager præcis det tilbage, den lagde, og lader dine lommeregnere være.

Downloadet er 37 MB og pakker ud til 98 MB, næsten det hele Qt. Den er bygget til **x86_64** mod **glibc 2.39**, altså Ubuntu 24.04, Mint 22, Debian 13 eller noget nyere. På en ældre distribution stopper den ved opstart med linjen `GLIBC_2.xx not found`, som ligner et nedbrud og ikke er det. Testet på X11.

### Android

1. Hent **[`Agape48-0.9.2-arm64-v8a.apk`](https://github.com/gbmaizol/Agape48/releases/latest)** ned på telefonen og tryk på den.
2. **Android nægter første gang** — *«your phone is not allowed to install unknown apps from this source»* — fordi filen ikke kom fra Play Store. Tryk **Settings**, tillad netop den kilde, og kom tilbage. Android spørger per *kilde*, så at tillade din browser tillader ikke også din filhåndtering.
3. Play Protect advarer muligvis derefter om en app fra en ukendt udvikler. Knappen **Install anyway** ligger bag *More details*.
4. Tryk **Open**. På en telefon fylder lommeregneren skærmen, ligesom Droid48 gør.

Begge de advarsler handler om *hvor filen kom fra*, ikke om hvad der er i den — og den anden er uundgåelig for enhver app, der ikke distribueres gennem Play Store.

Android **8.0 eller nyere**, og **arm64-v8a**, hvilket er enhver telefon solgt siden omkring 2016. En 32-bit telefon, eller et emulatorbillede bygget til x86_64, nægter at installere den.

## Giv den så en ROM, og det er det eneste fedtede trin

Det ene, der ikke er med i downloadet, er **ROM-billedet** — lommeregnerens egen firmware, den kode HP brændte ind i den rigtige maskine. Agape48 indeholder den ikke, fordi det er HP's kode og ikke vores.

Den er til gengæld et gratis download, og det er ikke et blink med øjet: **HP gav tilladelse til, at disse ROM'er måtte hentes, midt i år 2000**, og de har ligget på hpcalc.org lige siden. Det er det arkiv, hele HP-hobbyen bruger.

### Sådan får du en

1. Gå til **<https://www.hpcalc.org/hp48/pc/emulators/>**.
2. Den side er mest emulatorer, ikke ROM'er, og ROM-billederne ligger langt nede. Lad være med at rulle — tryk **Ctrl-F** (**⌘-F** på en Mac) og søg på siden efter **`HP 48GX Revision`**. Det lander dig på dem.
3. Der er elleve, og de virker alle sammen. Vil du have den, du kan holde op med at tænke på, så tag **`gxrom-r.zip`** — den sidste revision af 48GX, som er den lommeregner, denne forplade er tegnet efter. 314 KB.
4. **Pak den ud, og gør det før du går på jagt efter filen.** Indeni ligger én enkelt fil ved navn **`gxrom-r`** uden filendelse, 524.288 byte. Den fil *er* ROM'en: intet at konvertere, intet at pakke videre ud. En fil, der stadig ligger inde i `.zip`-filen, er usynlig for Agape48 og for enhver filvælger, hvilket er den mest sandsynlige grund til «jeg hentede den, og programmet kan ikke finde den».
5. Kan du bagefter ikke finde den udpakkede fil, **så sortér din Overførsler-mappe efter dato og kig på det ældste, der ligger der.** Disse ROM'er bærer deres oprindelige tidsstempler fra starten af 2000'erne, så den nyeste fil du har, er den, der ser femogtyve år gammel ud.

De øvrige, hvis du er nysgerrig snarere end på farten: `gxrom-k` til `gxrom-r` er 48GX-revisionerne K, L, M, P og R, og `sxrom-a` til `sxrom-j` er den ældre 48SX. Senere er generelt bedre — R rettede fejl, som K havde — men en 48SX-ROM gør Agape48 til en 48SX, hvilket er hele pointen med at have valget.

### Læg den hvor lommeregneren kigger

Begge veje virker, og ingen af dem har brug for den anden:

**Den sikre vej — omdøb og læg.** Omdøb filen til præcis **`rom`**, uden filendelse, og læg den i lommeregnerens egen mappe:

| | |
|---|---|
| Windows | `%LOCALAPPDATA%\Agape48\Agape48` |
| Linux | `~/.local/share/Agape48/Agape48` |
| Android | `Android/media/br.gbmaizol.agape48/Agape48 calculators` |

På Android er det klart den nemmeste af de to veje: mappen er synlig i enhver filhåndtering og over et USB-kabel og kræver slet ingen tilladelse, så du sætter telefonen til en computer og lægger filen ind.

**Den hurtige vej — peg appen på den.** Start Agape48 og klik på det understregede **48GX** i øverste højre hjørne af lommeregneren — det er menuen, og der er ingen anden vej ind. Derefter **Settings → HP 48 ROM**, og vælg filen, dér hvor den allerede ligger. Lommeregneren starter i det øjeblik du trykker Open, og stien huskes, så du gør det aldrig igen.

To krøller, begge om filvælgeren. Den kan ikke kigge ind i en `.zip`, så pak ud først. Og den viser ROM-agtige navne, hvilket omfatter `gxrom-r` og `sxrom-a` — men har du omdøbt filen til noget andet og ikke kan se den, så skift vælgerens filter til **All files**.

### Hvornår du gør det

Når som helst. Installér først og start den, og er der endnu ingen ROM, siger den det og **skriver den præcise mappe, den kiggede i** — så den ærlige rækkefølge er: installér, start den én gang, læs mappen af skærmen, læg filen ind, start den igen.

### Det første skærmbillede, så det ikke bekymrer dig

Med en frisk ROM spørger lommeregneren **`Try To Recover Memory?`**, fordi dens hukommelse aldrig er blevet skrevet. Det er den rigtige HP 48, der spørger, præcis som en ny ville. Tryk på den **højre af de seks blanke taster** øverst — det er **NO** — og du har en ren lommeregner.

Hvor den mappe ligger, er dit at flytte når som helst, og det er den samme indstilling, der lader én lommeregner ligge i Dropbox og blive åbnet fra begge maskiner.

## Fri software

Agape48 er fri software under **GNU General Public License, version 3**, og det er den nødt til at være: den står på Saturn-kernen `x48` skrevet af Eddie C. Dost og på de mange års arbejde, Droid48 lagde i den, begge under GPL. Dosts filer siger «version 2, eller enhver senere version», og version 3 er den senere, dette projekt tager — den version, der også forsvarer dig mod patentkrav og mod enhver, der leverer dette på en enhed, du ikke har lov at ændre. Så kildekoden ligger her, det hele, og alt hvad der bygges på den, forbliver frit på samme måde. Den fulde tekst står i `LICENSE`.

## For programmører

`Readme_Programmers.md` — bygningen, strukturen, størrelsesbudgettet, sømmen mellem Qt-forsiden og C-kernen, og den ene usædvanlige regel: **alt håndskrevet i dette projekt er på esperanto.** Kommentarerne, de pull requests der bliver merget, fejlrapporterne, issues. Læser du det ikke endnu, så argumenterer den side for to måneders aftener.
