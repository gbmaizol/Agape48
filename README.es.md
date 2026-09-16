# Agape48

[English](README.md) · [Dansk](README.da.md) · [Deutsch](README.de.md) · **Español** · [Esperanto](README.eo.md) · [Français](README.fr.md) · [Português](README.pt-BR.md)

**El HP 48 que ya amas, en la computadora y el teléfono que de verdad usas — y se lleva tu calculadora consigo.**

«HP» pronunciado en portugués de Brasil suena *agá-pê*, que es el griego ἀγάπη: amor. Agape48 lleva el nombre de la manera en que la gente habla de esta calculadora.

---

## Si nunca has usado una HP, lee primero esta parte

Casi todas las calculadoras que has tocado en tu vida funcionan así:

```
( 3 + 4 ) × 5 =
```

Tuviste que escribir dos paréntesis para decir lo que querías, y solo al final te enterabas de si era eso. Equivócate en uno y la respuesta sale mal, en silencio.

Una HP 48 funciona así:

```
3 ENTER 4 + 5 ×
```

Di lo que tienes, y después di qué hacer con ello. Eso es todo lo que es RPN. **Nunca vuelves a escribir un paréntesis**, y la calculadora te muestra cada resultado intermedio mientras ocurre — después de `4 +` ves el `7` ahí puesto, así que un error se ve en el momento en que lo cometes y no al final.

Se lee raro durante unos veinte minutos y después se lee como aritmética. Tres cosas pasan a partir de ahí:

- **Dejas de contar pulsaciones** — RPN suele pedir menos, y nunca más.
- **Dejas de empezar de nuevo.** Todo lo que has escrito se queda en una pila, visible, delante de ti. ¿Arruinaste el último número? Es una tecla para tirarlo, no volver a empezar.
- **Dejas de confiar en una sola línea.** Un cálculo largo en una calculadora normal es un número y una esperanza. Aquí son cuatro números que puedes mirar.

Si quieres la versión corta: escribe los números, pulsa `ENTER` entre ellos, y después pulsa la operación. Con eso ya se hace trabajo de verdad el primer día.

## Por qué esta

Hay emuladores de HP 48 desde hace treinta años, y son buenos — Agape48 está construido sobre el núcleo de uno de ellos. Lo que nunca hubo es *uno* de ellos que se comporte igual en tu portátil y en tu teléfono, y que se pase la calculadora del uno al otro.

| | Windows | Linux | Android | lee archivos de objeto de HP 48 | los escribe |
| --- | :---: | :---: | :---: | :---: | :---: |
| **Agape48** | **sí** | **sí** | **sí** | **sí** | **sí** |
| Emu48 | sí | — | — | sí | sí |
| Droid48 | — | — | sí | sí | — |
| x48 | — | sí | — | — | — |

Ese último par de columnas es el que importa en la práctica: un archivo de objeto de HP 48 es la forma en que un programa o una matriz se mueve entre todos estos y una calculadora real, y hasta ahora ningún programa por sí solo los leía y los escribía en todas partes. En Android no había nada que hiciera las dos cosas.

## Tu calculadora, no una copia de ella

La memoria de la calculadora es **una carpeta que eliges tú**. Pon esa carpeta en Dropbox, OneDrive, Syncthing, una memoria USB — cualquier cosa que sincronice archivos — y la calculadora que estabas usando en el portátil es la calculadora que se abre en el teléfono, con tus variables, tus programas y tu pila exactamente donde los dejaste.

Dos detalles honestos, porque esta es la parte que los demás emuladores te dejan descubrir solo:

- **Una a la vez.** La memoria de una calculadora es un montón con punteros hacia sí misma; dos dispositivos editándola a la vez no hay quien los fusione. Así que Agape48 pone una marca en la carpeta diciendo qué máquina la tiene abierta, avisa cuando otra la tiene, y **lo dice en vez de elegir un ganador en silencio**. Es el mismo trato que te da una base de datos de contraseñas, y por el mismo motivo.
- **De verdad son solo archivos.** Ninguna cuenta, ninguna nube nuestra, nada que llame a casa. Tu programa de sincronización ya era bueno en esto; Agape48 no intenta reemplazarlo.

Puedes tener **varias calculadoras en la misma carpeta** — una para el trabajo, una para el examen, una de la que no estás seguro — y cambiar con *Open another calculator…*. Comparten la ROM, así que la segunda cuesta casi nada.

## Tu teclado, tus teclas

Todo el teclado físico se puede reasignar, y la manera de averiguar qué hace una tecla es **pasar el ratón por encima**: la calculadora te dice qué teclas de tu teclado la pulsan.

- **Haz clic derecho en cualquier tecla** de la carátula para cambiarla. Pulsa la tecla que quieras. Listo.
- Dos nombres en una tecla de la calculadora significa que dos teclas de tu teclado llegan a ella, que suele ser lo que quieres — `Enter` y el `Enter` del teclado numérico pulsan los dos `ENTER`.
- En un teléfono no hay botón derecho, así que *Customize keyboard…* en el menú activa lo mismo para un toque normal.

Nada viene fijo. El mapa entero vive en un archivo de texto pequeño **junto a tu ROM**, en la misma carpeta que la memoria de la calculadora — así tus asignaciones viajan con la calculadora en vez de quedarse en una sola máquina.

## Tan rápida como quieras, incluido «nada»

Una HP 48 real corre a unos 2 MHz en silicio de 1990, y hay un interruptor para eso: **Slow down to real calculator speed** la deja tan lenta como la del cajón, a propósito, porque a veces la gracia es justamente la calculadora que recuerdas. Si no, un deslizador la sube hasta donde dé tu máquina, que es bastante más rápido que 1990 — útil la primera vez que corres un programa que antes daba para un café.

## Meter y sacar cosas

- **Copiar y pegar** con el resto de tu computadora, por el portapapeles del sistema.
- **Importar y exportar archivos de objeto de HP 48** — el formato `HPHP48-` — en los dos sentidos, que es como un programa se mueve a una 48 real, a Emu48, a Droid48, o de vuelta. Comprobado byte a byte contra Emu48 con una biblioteca de terceros como control: de 1206 bytes, 1205 volvieron idénticos, y el que difiere es el byte que *nombra la máquina que escribió el archivo*, que para eso está.
- **Save memory now** cuando quieras, y automáticamente cuando la aplicación pasa a segundo plano.

## Autónoma, y sin estorbar

Un archivo por plataforma, con su propia copia de Qt dentro, así que no hay ningún runtime que instalar antes. La ventana cambia de tamaño libremente y mantiene las proporciones de la calculadora; puedes dejarla como una tira fina al lado de tu trabajo o llenar la pantalla con ella. Los tamaños de texto son ajustables, porque una calculadora que tienes abierta todo el día es una calculadora que deberías poder leer.

## Instalarla

Una descarga por máquina, ningún runtime que buscar antes, y ninguna cuenta que crear.

**Las tres están en la [página de versiones](https://github.com/gbmaizol/Agape48/releases/latest)**, con un sha256 al lado de cada una por si quieres comprobar lo que bajaste.

Los avisos que se citan abajo aparecen en el idioma de tu sistema. Aquí están en inglés porque así es como los escriben Windows y Android en una instalación en inglés.

### Windows

1. Descarga **[`Agape48-0.9.3-windows-x64-setup.exe`](https://github.com/gbmaizol/Agape48/releases/latest)** (17,4 MB).
2. **Windows te va a frenar**: *«Windows protected your PC»*. Haz clic en **More info**, y después en **Run anyway**. Ese mensaje no es un aviso de virus — es Windows diciendo que el instalador no lleva certificado de firma de código, que es una compra y no un paso de compilación. Nada en la descarga está empaquetado ni ofuscado, y cada byte de código fuente que entró en ella está en este repositorio.
3. **Next**, **Next**, **Install**. Va a `Program Files` y añade una entrada al menú Inicio.
4. Ábrela desde el menú Inicio. Se desinstala como cualquier otro programa, desde *Apps & features*.

### Linux

Sin root, nada en `/opt`, y nada que añadir a tu gestor de paquetes. Toma **[`Agape48-0.9.3-linux-x86_64.tar.gz`](https://github.com/gbmaizol/Agape48/releases/latest)**, y después:

```sh
tar xzf Agape48-0.9.3-linux-x86_64.tar.gz
./Agape48-0.9.3-linux-x86_64/install.sh
```

No hay paso de `chmod`: `tar` conserva el bit de ejecución, así que `install.sh` simplemente corre. (Si lo descomprimiste con un archivador gráfico que perdió los permisos, `sh install.sh` funciona igual.)

Todo queda bajo `~/.local/share/agape48`, la entrada de menú aparece en **Education**, y escribir `agape48` la ejecuta si `~/.local/bin` está en tu `PATH`. El runtime de Qt viaja dentro del tarball, así que no hay paquete de distribución que perseguir ni nada que instalar antes — lo que también significa que no se puede romper cuando tu distribución pase al siguiente Qt. Para quitarla: `~/.local/share/agape48/uninstall.sh`, que recoge exactamente lo que dejó y no toca tus calculadoras.

La descarga son 36,0 MB y se descomprime a 98 MB, casi todo Qt. Está compilada para **x86_64** contra **glibc 2.39**, así que Ubuntu 24.04, Mint 22, Debian 13 o cualquier cosa más nueva. En una distribución más vieja se para al arrancar con una línea `GLIBC_2.xx not found`, que parece un fallo y no lo es. Probado en X11.

### Android

1. Descarga **[`Agape48-0.9.3-arm64-v8a.apk`](https://github.com/gbmaizol/Agape48/releases/latest)** (47,6 MB) en el teléfono y tócala.
2. **Android se va a negar la primera vez** — *«your phone is not allowed to install unknown apps from this source»* — porque el archivo no vino de la Play Store. Toca **Settings**, permite esa fuente concreta, y vuelve. Android pregunta por *fuente*, así que permitir tu navegador no permite también tu gestor de archivos.
3. Play Protect puede avisar luego de una aplicación de un desarrollador no reconocido. El botón **Install anyway** está detrás de *More details*.
4. Toca **Open**. En un teléfono la calculadora ocupa toda la pantalla, como hace Droid48.

Los dos avisos son sobre *de dónde vino el archivo*, no sobre lo que hay dentro — y el segundo es inevitable para cualquier aplicación que no se distribuya por la Play Store.

Android **8.0 o más nuevo**, y **arm64-v8a**, que es todo teléfono vendido desde alrededor de 2016. Un teléfono de 32 bits, o una imagen de emulador compilada para x86_64, se negará a instalarla.

## Y después dale una ROM, que es el único paso engorroso

Lo único que no viene en la descarga es la **imagen de la ROM** — el firmware de la propia calculadora, el código que HP grabó en la máquina real. Agape48 no la incluye, porque es código de HP y no nuestro.

Eso sí, es una descarga gratuita, y no es un guiño: **HP dio permiso para que estas ROM se descargaran a mediados del año 2000**, y están en hpcalc.org desde entonces. Ese es el archivo que usa toda la afición a las HP.

### Conseguir una

1. Ve a **<https://www.hpcalc.org/hp48/pc/emulators/>**.
2. Esa página es sobre todo emuladores, no ROM, y las imágenes están muy abajo. No bajes desplazándote — pulsa **Ctrl-F** (**⌘-F** en un Mac) y busca en la página **`HP 48GX Revision`**. Eso te deja justo en ellas.
3. Hay once, y cualquiera sirve. Si quieres aquella con la que dejar de pensar en el tema, toma **`gxrom-r.zip`** — la última revisión de la 48GX, que es la calculadora a partir de la cual está dibujada esta carátula. 314 KB.
4. **Descomprímela, y hazlo antes de ponerte a buscar el archivo.** Dentro hay un único archivo llamado **`gxrom-r`** sin extensión, de 524.288 bytes. Ese archivo *es* la ROM: nada que convertir, nada que desempaquetar más. Un archivo que sigue dentro del `.zip` es invisible para Agape48 y para cualquier diálogo de archivos, que es el motivo más probable de «la descargué y el programa no la encuentra».
5. Si después no encuentras el archivo descomprimido, **ordena tu carpeta de Descargas por fecha y mira lo más viejo que haya.** Estas ROM llevan sus fechas originales de principios de los 2000, así que el archivo más nuevo que tienes es el que parece de hace veinticinco años.

Las demás, si tienes curiosidad más que prisa: `gxrom-k` a `gxrom-r` son las revisiones K, L, M, P y R de la 48GX, y `sxrom-a` a `sxrom-j` son la 48SX más antigua. Más tarde suele ser mejor — la R corrigió fallos que tenía la K — pero una ROM de 48SX convierte Agape48 en una 48SX, que es justamente la gracia de poder elegir.

### Ponerla donde la calculadora mira

Las dos formas funcionan, y ninguna necesita la otra:

**La forma segura — renombra y suelta.** Renombra el archivo a exactamente **`rom`**, sin extensión, y ponlo en la carpeta de la propia calculadora:

| | |
|---|---|
| Windows | `%LOCALAPPDATA%\Agape48\Agape48` |
| Linux | `~/.local/share/Agape48/Agape48` |
| Android | `Android/media/br.gbmaizol.agape48/Agape48 calculators` |

En Android esa es con diferencia la más fácil de las dos: la carpeta se ve en cualquier gestor de archivos y por cable USB y no necesita permiso ninguno, así que conectas el teléfono a una computadora y sueltas el archivo ahí.

**La forma rápida — apunta la aplicación al archivo.** Abre Agape48 y haz clic en el **48GX** subrayado en la esquina superior derecha de la calculadora — ese es el menú, y no hay otra entrada. Después **Settings → HP 48 ROM**, y elige el archivo donde ya está. Elegirlo solo rellena el campo: **Save** es lo que lo carga, y la calculadora pasa a esa ROM en la misma ventana, sin reiniciarse. Si el archivo no es una ROM de HP 48, la ruta se vuelve roja y la línea de debajo dice por qué, y Save sigue gris hasta que ahí haya una ROM. La ruta se recuerda, así que no lo vuelves a hacer.

Dos pliegues, los dos sobre el diálogo de archivos. No puede mirar dentro de un `.zip`, así que descomprime primero. Y lista nombres con forma de ROM, lo que incluye `gxrom-r` y `sxrom-a` — pero si renombraste el archivo a otra cosa y no lo ves, cambia el filtro del diálogo a **All files**.

### Cuándo hacerlo

Cuando quieras. Instala primero y ábrela, y si todavía no hay ROM lo dice y **escribe la carpeta exacta en la que miró** — así que el orden honesto es: instala, ábrela una vez, lee la carpeta en la pantalla, suelta el archivo ahí, y ábrela de nuevo.

### La primera pantalla, para que no te preocupe

Con una ROM recién puesta la calculadora pregunta **`Try To Recover Memory?`**, porque su memoria nunca se ha escrito. Es la HP 48 real preguntando, exactamente como haría una nueva. Pulsa la **más a la derecha de las seis teclas en blanco** de arriba — esa es **NO** — y tienes una calculadora limpia.

Dónde vive esa carpeta es cosa tuya y se puede mover cuando quieras, y es el mismo ajuste que deja que una calculadora esté en Dropbox y se abra desde las dos máquinas.

## Software libre

Agape48 es software libre bajo la **Licencia Pública General de GNU, versión 3**, y tiene que serlo: está sobre el núcleo Saturn `x48` escrito por Eddie C. Dost y sobre los años de trabajo que Droid48 puso en él, ambos bajo la GPL. Los archivos de Dost dicen «versión 2, o cualquier versión posterior», y la versión 3 es la posterior que toma este proyecto — la versión que además te defiende frente a reclamaciones de patentes y frente a quien distribuya esto en un aparato que no tienes derecho a modificar. Así que el código fuente está aquí, entero, y todo lo que se construya sobre él sigue siendo libre igual. El texto completo está en `LICENSE`.

## Para programadores

`Readme_Programmers.md` — la compilación, la estructura, el presupuesto de tamaño, la costura entre la interfaz en Qt y el núcleo en C, y la única regla poco habitual: **todo lo escrito a mano en este proyecto está en esperanto.** Los comentarios, los pull requests que se fusionan, los informes de fallos, las issues. Si todavía no lo lees, esa página es el argumento a favor de dos meses de tardes.
