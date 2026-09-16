# Agape48

[English](README.md) · [Dansk](README.da.md) · [Deutsch](README.de.md) · [Español](README.es.md) · **Esperanto** · [Français](README.fr.md) · [Português](README.pt-BR.md)

**La HP 48 kiun vi jam amas, sur la komputilo kaj la telefono kiujn vi vere uzas — kaj ĝi kunportas vian kalkulilon.**

«HP» elparolata en la brazila portugala sonas *agá-pê*, kio estas la greka ἀγάπη: amo. Agape48 portas la nomon de la maniero kiel oni parolas pri ĉi tiu kalkulilo.

---

## Se vi neniam uzis HP-on, legu unue ĉi tiun parton

Preskaŭ ĉiu kalkulilo kiun vi iam tuŝis funkcias tiel:

```
( 3 + 4 ) × 5 =
```

Vi devis tajpi du krampojn por diri kion vi celis, kaj nur ĉe la fino vi eksciis ĉu tio estis kion vi celis. Unu krampo malĝusta, kaj la respondo estas malĝusta, silente.

HP 48 funkcias tiel:

```
3 ENTER 4 + 5 ×
```

Diru kion vi havas, poste diru kion fari per ĝi. Tio estas la tuta RPN. **Vi neniam plu tajpas krampon**, kaj la kalkulilo montras ĉiun intertempan rezulton dum ĝi aperas — post `4 +` la `7` sidas tie videbla, do eraro montriĝas en la momento kiam vi faras ĝin anstataŭ ĉe la fino.

Ĝi legiĝas strange dum ĉirkaŭ dudek minutoj kaj poste ĝi legiĝas kiel aritmetiko. Tri aferoj okazas post tio:

- **Vi ĉesas kalkuli klavopremojn** — RPN kutime bezonas malpli da ili, kaj neniam pli.
- **Vi ĉesas rekomenci.** Ĉio kion vi tajpis restas sur stako, videbla, antaŭ vi. Fuŝis la lastan numeron? Unu klavo forĵetas ĝin, ne nova komenco.
- **Vi ĉesas fidi al unu sola linio.** Longa kalkulo sur ordinara kalkulilo estas unu numero kaj unu espero. Ĉi tie ĝi estas kvar numeroj kiujn vi povas rigardi.

Se vi volas la mallongan version: tajpu la numerojn, premu `ENTER` inter ili, poste premu la operacion. Tio sufiĉas por fari veran laboron jam en la unua tago.

## Kial ĝuste ĉi tiu

HP 48-emuliloj ekzistas de tridek jaroj, kaj ili estas bonaj — Agape48 staras sur la kerno de unu el ili. Kio neniam ekzistis estas *unu* el ili kiu kondutas same sur via tekokomputilo kaj via telefono, kaj transdonas la kalkulilon inter ili.

| | Windows | Linux | Android | legas HP 48-objektdosierojn | skribas ilin |
| --- | :---: | :---: | :---: | :---: | :---: |
| **Agape48** | **jes** | **jes** | **jes** | **jes** | **jes** |
| Emu48 | jes | — | — | jes | jes |
| Droid48 | — | — | jes | jes | — |
| x48 | — | jes | — | — | — |

Tiu lasta paro da kolumnoj estas la grava en la praktiko: HP 48-objektdosiero estas la maniero kiel programo aŭ matrico moviĝas inter ĉiuj ĉi tiuj kaj vera kalkulilo, kaj ĝis nun neniu sola programo kapablis kaj legi kaj skribi ilin ĉie. Sur Android nenio faris ambaŭ entute.

## Via kalkulilo, ne kopio de ĝi

La memoro de la kalkulilo estas **dosierujo kiun vi elektas**. Metu tiun dosierujon en Dropbox, OneDrive, Syncthing, memorbastoneton — ion ajn kio sinkronigas dosierojn — kaj la kalkulilo kiun vi uzis sur la tekokomputilo estas la kalkulilo kiu malfermiĝas sur la telefono, kun viaj variabloj, viaj programoj kaj via stako ĝuste tie kie vi lasis ilin.

Du honestaj detaloj, ĉar ĉi tiun parton la aliaj emuliloj lasas al vi mem malkovri:

- **Unu samtempe.** La memoro de kalkulilo estas amaso kun montriloj en si mem; du aparatoj redaktantaj ĝin samtempe estas kunfandeblaj de neniu. Do Agape48 metas markilon en la dosierujon dirantan kiu maŝino tenas ĝin malfermita, avertas vin kiam alia tenas ĝin, kaj **diras tion anstataŭ silente elekti venkinton**. Estas la sama interkonsento kiun pasvortdatumbazo donas al vi, kaj pro la sama kialo.
- **Ĝi vere estas nur dosieroj.** Neniu konto, neniu nubo nia, nenio telefonanta hejmen. Via sinkroniga programo jam bone faras tion; Agape48 ne provas anstataŭi ĝin.

Vi povas teni **plurajn kalkulilojn en la sama dosierujo** — unu por la laboro, unu por la ekzameno, unu pri kiu vi ne certas — kaj ŝanĝi per *Open another calculator…*. Ili kunhavas la ROM-on, do la dua kostas preskaŭ nenion.

## Via klavaro, viaj klavoj

La tuta fizika klavaro estas remapebla, kaj la maniero ekscii kion klavo faras estas **ŝvebi super ĝi**: la kalkulilo diras kiuj klavoj de via klavaro premas ĝin.

- **Dekstre alklaku iun ajn klavon** sur la vizaĝo por ŝanĝi ĝin. Premu la klavon kiun vi volas. Farite.
- Du nomoj sur unu kalkulila klavo signifas ke du klavoj de via klavaro atingas ĝin, kio kutime estas ĝuste kion vi volas — `Enter` kaj la ciferklavara `Enter` ambaŭ premas `ENTER`.
- Sur telefono ne ekzistas dekstra butono, do *Customize keyboard…* en la menuo ŝaltas la samon por simpla frapeto.

Nenio estas fiksita interne. La tuta mapo loĝas en malgranda tekstdosiero **apud via ROM**, en la sama dosierujo kiel la memoro de la kalkulilo — do viaj klavligoj vojaĝas kun la kalkulilo anstataŭ resti sur unu maŝino.

## Tiel rapida kiel vi volas, inkluzive de «ne»

Vera HP 48 kuras je ĉirkaŭ 2 MHz en silicio de 1990, kaj por tio ekzistas ŝaltilo: **Slow down to real calculator speed** faras ĝin tiel malrapida kiel tiu en la tirkesto, intence, ĉar foje la afero estas ĝuste la kalkulilo kiun vi memoras. Alie ŝovbutono levas ĝin ĝis la rapido de via maŝino, kio estas multe pli rapida ol 1990 — utila la unuan fojon kiam vi rulas programon kiu antaŭe bezonis kafopaŭzon.

## Enigi kaj eligi aferojn

- **Kopiu kaj algluu** kun la cetero de via komputilo, tra la sistema tondujo.
- **Importu kaj eksportu HP 48-objektdosierojn** — la formato `HPHP48-` — en ambaŭ direktoj, kio estas la maniero kiel programo moviĝas al vera 48, al Emu48, al Droid48, aŭ reen. Kontrolite bajton post bajto kontraŭ Emu48, kun ekstera biblioteko kiel kontrolilo: el 1206 bajtoj, 1205 revenis identaj, kaj la unu kiu diferencas estas la bajto kiu *nomas la maŝinon kiu skribis la dosieron*, kiel ĝi devas.
- **Save memory now** kiam ajn vi volas, kaj aŭtomate kiam la aplikaĵo iras al la fono.

## Memstara, kaj ne baranta la vojon

Unu dosiero por ĉiu platformo, kun propra kopio de Qt interne, do neniu rultempo estas instalenda antaŭe. La fenestro libere regrandiĝas kaj konservas la proporciojn de la kalkulilo; vi povas fari ĝin maldika strio apud via laboro aŭ plenigi la ekranon per ĝi. La tekstgrandoj estas agordeblaj, ĉar kalkulilo kiun vi tenas malfermita la tutan tagon estas kalkulilo kiun vi devus povi legi.

## Instali ĝin

Unu elŝuto por ĉiu maŝino, neniu rultempo prenenda antaŭe, kaj neniu konto kreenda.

**Ĉiuj tri loĝas sur la [eldonpaĝo](https://github.com/gbmaizol/Agape48/releases/latest)**, kun sha256 apud ĉiu, se vi volas kontroli kion vi ricevis.

La averto-tekstoj citataj sube aperas en la lingvo de via sistemo. Ili staras ĉi tie en la angla ĉar tio estas kiel Windows kaj Android skribas ilin en angla instalaĵo.

### Windows

1. Elŝutu **[`Agape48-0.9.3-windows-x64-setup.exe`](https://github.com/gbmaizol/Agape48/releases/latest)** (17,3 MB).
2. **Windows haltigos vin**: *«Windows protected your PC»*. Klaku **More info**, poste **Run anyway**. Tiu mesaĝo ne estas virusaverto — ĝi estas Windows dirante ke la instalilo portas neniun kodsubskriban atestilon, kio estas aĉeto prefere ol konstrupaŝo. Nenio en la elŝuto estas pakita aŭ obskurigita, kaj ĉiu bajto de fonto kiu eniris ĝin troviĝas en ĉi tiu deponejo.
3. **Next**, **Next**, **Install**. Ĝi iras en `Program Files` kaj aldonas eron al la Komenca menuo.
4. Startigu ĝin el la Komenca menuo. Ĝi malinstaliĝas kiel ĉiu alia programo, el *Apps & features*.

### Linux

Neniu `root`, nenio en `/opt`, kaj nenio aldonenda al via pakadministrilo. Prenu **[`Agape48-0.9.3-linux-x86_64.tar.gz`](https://github.com/gbmaizol/Agape48/releases/latest)**, poste:

```sh
tar xzf Agape48-0.9.3-linux-x86_64.tar.gz
./Agape48-0.9.3-linux-x86_64/install.sh
```

Ne estas `chmod`-paŝo: `tar` konservas la plenumbiton, do `install.sh` simple ruliĝas. (Se vi malpakis per grafika arkivilo kiu forĵetis la permesojn, `sh install.sh` funkcias tamen.)

Ĉio alteriĝas sub `~/.local/share/agape48`, la menuero aperas sub **Education**, kaj tajpi `agape48` rulas ĝin se `~/.local/bin` estas sur via `PATH`. La Qt-rultempo vojaĝas interne de la tar-arkivo, do ne estas distribua pakaĵo ĉasenda kaj nenio instalenda antaŭe — kio ankaŭ signifas ke ĝi ne povas rompiĝi kiam via distribuo transiras al la sekva Qt. Por forigi ĝin: `~/.local/share/agape48/uninstall.sh`, kiu reprenas ĝuste tion kion ĝi demetis kaj lasas viajn kalkulilojn netuŝitaj.

La elŝuto estas 35,9 MB kaj malpakiĝas al 96 MB, preskaŭ tute Qt. Ĝi estas konstruita por **x86_64** kontraŭ **glibc 2.39**, do Ubuntu 24.04, Mint 22, Debian 13 aŭ io ajn pli nova. Sur pli malnova distribuo ĝi haltas je la starto kun linio `GLIBC_2.xx not found`, kiu legiĝas kiel kraŝo kaj ne estas tia. Testita sur X11.

### Android

1. Elŝutu **[`Agape48-0.9.3-arm64-v8a.apk`](https://github.com/gbmaizol/Agape48/releases/latest)** (47,4 MB) sur la telefonon kaj frapetu ĝin.
2. **Android rifuzos la unuan fojon** — *«your phone is not allowed to install unknown apps from this source»* — ĉar la dosiero ne venis el la Play Store. Frapetu **Settings**, permesu tiun unu fonton, kaj revenu. Android demandas laŭ *fonto*, do permesi vian retumilon ne samtempe permesas vian dosieradministrilon.
3. Play Protect poste eble avertos pri aplikaĵo de nerekonata programisto. La butono **Install anyway** kaŝiĝas malantaŭ *More details*.
4. Frapetu **Open**. Sur telefono la kalkulilo plenigas la ekranon, kiel Droid48 faras.

Ambaŭ tiuj avertoj temas pri *de kie la dosiero venis*, ne pri kio estas interne — kaj la dua estas neevitebla por ĉiu aplikaĵo kiu ne disdoniĝas tra la Play Store.

Android **8.0 aŭ pli nova**, kaj **arm64-v8a**, kio estas ĉiu telefono vendita ekde ĉirkaŭ 2016. 32-bita telefono, aŭ emulila bildo konstruita por x86_64, rifuzos instali ĝin.

## Poste donu al ĝi ROM-on, kaj tio estas la sola ĝena paŝo

La sola afero ne enhavata en la elŝuto estas la **ROM-bildo** — la propra mikroprogramaro de la kalkulilo, la kodo kiun HP bruligis en la veran maŝinon. Agape48 ne enhavas ĝin, ĉar ĝi estas la kodo de HP kaj ne nia.

Ĝi tamen estas senpaga elŝuto, kaj tio ne estas palpebrumo: **HP donis permeson elŝuti ĉi tiujn ROM-ojn meze de la jaro 2000**, kaj ili staras sur hpcalc.org de tiam. Tio estas la arkivo kiun la tuta HP-hobio uzas.

### Akiri unu

1. Iru al **<https://www.hpcalc.org/hp48/pc/emulators/>**.
2. Tiu paĝo estas plejparte emuliloj, ne ROM-oj, kaj la ROM-bildoj kuŝas longe sube. Ne rulumu — premu **Ctrl-F** (**⌘-F** sur Mac) kaj serĉu en la paĝo **`HP 48GX Revision`**. Tio metas vin ĝuste sur ilin.
3. Estas dek unu, kaj ĉiu el ili funkcias. Se vi volas tiun pri kiu vi ĉesu pensi, prenu **`gxrom-r.zip`** — la lasta revizio de la 48GX, kiu estas la kalkulilo laŭ kiu ĉi tiu vizaĝo estas desegnita. 314 KB.
4. **Malzipu ĝin, kaj faru tion antaŭ ol vi ekserĉas la dosieron.** Interne estas unu sola dosiero nomata **`gxrom-r`** sen finaĵo, 524 288 bajtoj. Tiu dosiero *estas* la ROM: nenio konvertenda, nenio plu malpakenda. Dosiero ankoraŭ sidanta interne de la `.zip` estas nevidebla por Agape48 kaj por ĉiu dosierdialogo, kio estas la plej verŝajna kialo de «mi elŝutis ĝin kaj la programo ne trovas ĝin».
5. Se vi poste ne trovas la malzipitan dosieron, **ordigu vian Elŝut-dosierujon laŭ dato kaj rigardu la plej malnovan aferon en ĝi.** Ĉi tiuj ROM-oj portas siajn originalajn tempindikojn el la fruaj 2000-aj jaroj, do la plej nova dosiero kiun vi havas estas tiu kiu aspektas dudekkvinjara.

La ceteraj, se vi estas scivola prefere ol prema: `gxrom-k` ĝis `gxrom-r` estas la 48GX-revizioj K, L, M, P kaj R, kaj `sxrom-a` ĝis `sxrom-j` estas la pli malnova 48SX. Pli posta estas ĝenerale pli bona — R riparis cimojn kiujn K havis — sed 48SX-ROM turnas Agape48 en 48SX-on, kio estas ĝuste la senco havi la elekton.

### Meti ĝin kien la kalkulilo rigardas

Ambaŭ vojoj funkcias, kaj neniu el ili bezonas la alian:

**La certa vojo — renomu kaj demetu.** Renomu la dosieron al ĝuste **`rom`**, sen finaĵo, kaj metu ĝin en la propran dosierujon de la kalkulilo:

| | |
|---|---|
| Windows | `%LOCALAPPDATA%\Agape48\Agape48` |
| Linux | `~/.local/share/Agape48/Agape48` |
| Android | `Android/media/br.gbmaizol.agape48/Agape48 calculators` |

Sur Android tio estas multe la pli facila el la du vojoj: la dosierujo estas videbla en ĉiu dosieradministrilo kaj tra USB-kablo kaj bezonas nenian permeson, do vi konektas la telefonon al komputilo kaj demetas la dosieron tien.

**La rapida vojo — montru al la aplikaĵo kie ĝi estas.** Startigu Agape48 kaj klaku la substrekitan **48GX** en la supra dekstra angulo de la kalkulilo — tio estas la menuo, kaj alia enirejo ne ekzistas. Poste **Settings → HP 48 ROM**, kaj elektu la dosieron tie kie ĝi jam kuŝas. La elekto nur plenigas la kampon: **Save** estas tio, kio ŝargas ĝin, kaj la kalkulilo transiras al tiu ROM en la sama fenestro, sen restarto. Se la dosiero ne estas ROM de HP 48, la vojo ruĝiĝas kaj la linio sub ĝi diras kial, kaj Save restas griza ĝis tie staras ROM. La vojo estas memorata, do vi neniam refaras tion.

Du faldetoj, ambaŭ pri la dosierdialogo. Ĝi ne povas rigardi internen de `.zip`, do malzipu unue. Kaj ĝi listigas ROM-formajn nomojn, kio inkluzivas `gxrom-r` kaj `sxrom-a` — sed se vi renomis la dosieron al io alia kaj ne vidas ĝin, ŝanĝu la filtrilon de la dialogo al **All files**.

### Kiam fari tion

Kiam ajn vi volas. Instalu unue kaj startigu ĝin, kaj se ankoraŭ ne estas ROM ĝi diras tion kaj **presas la ĝustan dosierujon en kiun ĝi rigardis** — do la honesta ordo estas: instalu, startigu unufoje, legu la dosierujon de sur la ekrano, demetu la dosieron tien, startigu denove.

### La unua ekrano, por ke ĝi ne maltrankviligu vin

Kun freŝa ROM la kalkulilo demandas **`Try To Recover Memory?`**, ĉar ĝia memoro neniam estis skribita. Tio estas la vera HP 48 demandanta, ĝuste kiel nova farus. Premu la **plej dekstran el la ses blankaj klavoj** laŭ la supro — tio estas **NO** — kaj vi havas puran kalkulilon.

Kie tiu dosierujo kuŝas estas via afero movebla je ĉiu momento, kaj tio estas la sama agordo kiu lasas unu kalkulilon sidi en Dropbox kaj esti malfermata de ambaŭ maŝinoj.

## Libera programaro

Agape48 estas libera programaro laŭ la **Ĝenerala Publika Permesilo de GNU, versio 3**, kaj ĝi devas esti tia: ĝi staras sur la Saturn-kerno `x48` verkita de Eddie C. Dost kaj sur la jaroj da laboro kiujn Droid48 metis en ĝin, ambaŭ laŭ la GPL. La dosieroj de Dost diras «versio 2, aŭ iu ajn pli posta versio», kaj versio 3 estas la pli posta kiun ĉi tiu projekto prenas — la versio kiu ankaŭ defendas vin kontraŭ patentpretendoj kaj kontraŭ liverado de ĉi tio sur aparato kiun vi ne rajtas ŝanĝi. Do la fonto estas ĉi tie, la tuta, kaj ĉio konstruita sur ĝi restas libera same. La plena teksto troviĝas en `LICENSE`.

## Por programistoj

`Readme_Programmers.md` — la konstruo, la aranĝo, la grandobuĝeto, la kunkudro inter la Qt-fasado kaj la C-kerno, kaj la unu nekutima regulo: **ĉio manskribita en ĉi tiu projekto estas en Esperanto.** La komentoj, la kunfandataj tirpetoj, la cimraportoj, la problemoj. Vi do jam povas legi la tutan fonton. Tiu paĝo mem estas en la angla, kaj tio estas intenca: ĝi devas klarigi la regulon al tiuj kiuj ankoraŭ ne povas legi ĝin.
