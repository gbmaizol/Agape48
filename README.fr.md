# Agape48

[English](README.md) · [Dansk](README.da.md) · [Deutsch](README.de.md) · [Español](README.es.md) · [Esperanto](README.eo.md) · **Français** · [Português](README.pt-BR.md)

**La HP 48 que vous aimez déjà, sur l'ordinateur et le téléphone que vous utilisez vraiment — et elle emporte votre calculatrice avec elle.**

« HP » prononcé en portugais du Brésil donne *agá-pê*, qui est le grec ἀγάπη : l'amour. Agape48 porte le nom de la façon dont les gens parlent de cette calculatrice.

---

## Si vous n'avez jamais utilisé une HP, lisez d'abord cette partie

Presque toutes les calculatrices que vous avez eues entre les mains fonctionnent comme ceci :

```
( 3 + 4 ) × 5 =
```

Il a fallu taper deux parenthèses pour dire ce que vous vouliez, et vous n'avez su qu'à la toute fin si c'était bien cela. Trompez-vous sur l'une des deux et le résultat est faux, sans un mot.

Une HP 48 fonctionne comme ceci :

```
3 ENTER 4 + 5 ×
```

Dites ce que vous avez, puis dites quoi en faire. C'est tout ce qu'est la RPN, la notation polonaise inverse. **Vous ne tapez plus jamais une parenthèse**, et la calculatrice vous montre chaque résultat intermédiaire au moment où il arrive — après `4 +` vous voyez le `7` posé là, donc une erreur se voit à l'instant où vous la faites, et non à la fin.

Cela se lit bizarrement pendant une vingtaine de minutes, puis cela se lit comme du calcul. Trois choses arrivent ensuite :

- **Vous arrêtez de compter les touches** — la RPN en demande généralement moins, et jamais plus.
- **Vous arrêtez de recommencer.** Tout ce que vous avez tapé reste sur une pile, visible, devant vous. Le dernier nombre est raté ? C'est une touche pour le jeter, pas un nouveau départ.
- **Vous arrêtez de faire confiance à une seule ligne.** Un long calcul sur une calculatrice ordinaire, c'est un nombre et un espoir. Ici, ce sont quatre nombres que vous pouvez regarder.

Si vous voulez la version courte : tapez les nombres, appuyez sur `ENTER` entre eux, puis appuyez sur l'opération. Cela suffit pour faire du vrai travail dès le premier jour.

## Pourquoi celle-ci

Les émulateurs de HP 48 existent depuis trente ans, et ils sont bons — Agape48 est construit sur le cœur de l'un d'eux. Ce qui n'a jamais existé, c'est *un seul* d'entre eux qui se comporte de la même façon sur votre portable et sur votre téléphone, et qui se passe la calculatrice de l'un à l'autre.

| | Windows | Linux | Android | lit les fichiers objet HP 48 | les écrit |
| --- | :---: | :---: | :---: | :---: | :---: |
| **Agape48** | **oui** | **oui** | **oui** | **oui** | **oui** |
| Emu48 | oui | — | — | oui | oui |
| Droid48 | — | — | oui | oui | — |
| x48 | — | oui | — | — | — |

Ce dernier couple de colonnes est celui qui compte en pratique : un fichier objet HP 48, c'est la façon dont un programme ou une matrice circule entre tous ceux-ci et une vraie calculatrice, et jusqu'ici aucun programme à lui seul ne savait les lire et les écrire partout. Sur Android, rien ne faisait les deux du tout.

## Votre calculatrice, pas une copie d'elle

La mémoire de la calculatrice est **un dossier que vous choisissez**. Mettez ce dossier dans Dropbox, OneDrive, Syncthing, sur une clé USB — n'importe quoi qui synchronise des fichiers — et la calculatrice que vous utilisiez sur le portable est la calculatrice qui s'ouvre sur le téléphone, avec vos variables, vos programmes et votre pile exactement là où vous les avez laissés.

Deux détails honnêtes, parce que c'est la partie que les autres émulateurs vous laissent découvrir tout seul :

- **Une seule à la fois.** La mémoire d'une calculatrice est un tas avec des pointeurs vers lui-même ; deux appareils qui la modifient en même temps, personne ne peut les fusionner. Alors Agape48 pose une marque dans le dossier disant quelle machine l'a ouvert, prévient quand une autre l'a, et **le dit au lieu de désigner un gagnant en silence**. C'est le même contrat qu'une base de mots de passe vous offre, et pour la même raison.
- **Ce ne sont vraiment que des fichiers.** Aucun compte, aucun nuage à nous, rien qui téléphone à la maison. Votre logiciel de synchronisation était déjà bon à cela ; Agape48 n'essaie pas de le remplacer.

Vous pouvez garder **plusieurs calculatrices dans le même dossier** — une pour le travail, une pour l'examen, une dont vous n'êtes pas sûr — et changer avec *Open another calculator…*. Elles partagent la ROM, donc la deuxième ne coûte presque rien.

## Votre clavier, vos touches

Tout le clavier physique est reconfigurable, et la façon de découvrir ce que fait une touche est de **passer la souris dessus** : la calculatrice vous dit quelles touches de votre clavier l'actionnent.

- **Clic droit sur n'importe quelle touche** de la façade pour la changer. Appuyez sur la touche que vous voulez. Terminé.
- Deux noms sur une touche de la calculatrice veut dire que deux touches de votre clavier l'atteignent, ce qui est généralement ce que vous voulez — `Entrée` et l'`Entrée` du pavé numérique actionnent toutes deux `ENTER`.
- Sur un téléphone il n'y a pas de bouton droit, alors *Customize keyboard…* dans le menu active la même chose pour une simple pression.

Rien n'est figé. La carte entière vit dans un petit fichier texte **à côté de votre ROM**, dans le même dossier que la mémoire de la calculatrice — donc vos affectations voyagent avec la calculatrice au lieu de rester sur une seule machine.

## Aussi rapide que vous voulez, y compris « pas du tout »

Une vraie HP 48 tourne à environ 2 MHz dans du silicium de 1990, et il y a un interrupteur pour cela : **Slow down to real calculator speed** la rend aussi lente que celle du tiroir, exprès, parce que parfois ce qu'on veut, c'est justement la calculatrice dont on se souvient. Sinon un curseur la pousse jusqu'à ce que votre machine peut donner, ce qui est bien plus rapide que 1990 — utile la première fois que vous lancez un programme qui prenait le temps d'un café.

## Faire entrer et sortir des choses

- **Copier et coller** avec le reste de votre ordinateur, par le presse-papiers du système.
- **Importer et exporter des fichiers objet HP 48** — le format `HPHP48-` — dans les deux sens, ce qui est la façon dont un programme rejoint une vraie 48, Emu48, Droid48, ou revient. Vérifié octet par octet contre Emu48, avec une bibliothèque tierce comme témoin : sur 1206 octets, 1205 sont revenus identiques, et celui qui diffère est l'octet qui *nomme la machine ayant écrit le fichier*, ce qu'il est censé faire.
- **Save memory now** quand vous le voulez, et automatiquement quand l'application passe en arrière-plan.

## Autonome, et pas dans le passage

Un fichier par plateforme, avec sa propre copie de Qt à l'intérieur, donc aucun environnement d'exécution à installer d'abord. La fenêtre se redimensionne librement et garde les proportions de la calculatrice ; vous pouvez en faire une bande étroite à côté de votre travail ou remplir l'écran avec. Les tailles de texte sont réglables, parce qu'une calculatrice que vous laissez ouverte toute la journée est une calculatrice que vous devriez pouvoir lire.

## L'installer

Un téléchargement par machine, aucun environnement d'exécution à récupérer d'abord, et aucun compte à créer.

**Les trois sont sur la [page des versions](https://github.com/gbmaizol/Agape48/releases/latest)**, avec un sha256 à côté de chacun si vous voulez vérifier ce que vous avez reçu.

Les avertissements cités ci-dessous apparaissent dans la langue de votre système. Ils sont ici en anglais parce que c'est ainsi que Windows et Android les écrivent sur une installation anglaise.

### Windows

1. Téléchargez **[`Agape48-0.9.2-windows-x64-setup.exe`](https://github.com/gbmaizol/Agape48/releases/latest)** (17,3 Mo).
2. **Windows va vous arrêter** : *« Windows protected your PC »*. Cliquez sur **More info**, puis sur **Run anyway**. Ce message n'est pas une alerte antivirus — c'est Windows qui dit que l'installateur ne porte aucun certificat de signature de code, ce qui est un achat et non une étape de compilation. Rien dans le téléchargement n'est empaqueté ni obscurci, et chaque octet de source qui y est entré se trouve dans ce dépôt.
3. **Next**, **Next**, **Install**. Cela va dans `Program Files` et ajoute une entrée au menu Démarrer.
4. Lancez-la depuis le menu Démarrer. Elle se désinstalle comme n'importe quel autre programme, depuis *Apps & features*.

### Linux

Pas de root, rien dans `/opt`, et rien à ajouter à votre gestionnaire de paquets. Prenez **[`Agape48-0.9.2-linux-x86_64.tar.gz`](https://github.com/gbmaizol/Agape48/releases/latest)**, puis :

```sh
tar xzf Agape48-0.9.2-linux-x86_64.tar.gz
./Agape48-0.9.2-linux-x86_64/install.sh
```

Il n'y a pas d'étape `chmod` : `tar` conserve le bit d'exécution, donc `install.sh` se lance tel quel. (Si vous avez décompressé avec un archiveur graphique qui a perdu les permissions, `sh install.sh` marche quand même.)

Tout atterrit sous `~/.local/share/agape48`, l'entrée de menu apparaît dans **Education**, et taper `agape48` la lance si `~/.local/bin` est dans votre `PATH`. L'environnement Qt voyage à l'intérieur de l'archive, donc il n'y a aucun paquet de distribution à chasser et rien à installer d'abord — ce qui veut aussi dire qu'elle ne peut pas casser quand votre distribution passera au Qt suivant. Pour la retirer : `~/.local/share/agape48/uninstall.sh`, qui reprend exactement ce qu'il a posé et laisse vos calculatrices tranquilles.

Le téléchargement fait 35,9 Mo et se décompresse en 96 Mo, presque entièrement du Qt. Il est compilé pour **x86_64** contre la **glibc 2.39**, donc Ubuntu 24.04, Mint 22, Debian 13 ou quoi que ce soit de plus récent. Sur une distribution plus ancienne il s'arrête au démarrage avec une ligne `GLIBC_2.xx not found`, qui ressemble à un plantage et n'en est pas un. Testé sous X11.

### Android

1. Téléchargez **[`Agape48-0.9.2-arm64-v8a.apk`](https://github.com/gbmaizol/Agape48/releases/latest)** (47,4 Mo) sur le téléphone et touchez-le.
2. **Android va refuser la première fois** — *« your phone is not allowed to install unknown apps from this source »* — parce que le fichier ne vient pas du Play Store. Touchez **Settings**, autorisez cette source-là, et revenez. Android demande par *source*, donc autoriser votre navigateur n'autorise pas aussi votre gestionnaire de fichiers.
3. Play Protect peut ensuite avertir au sujet d'une application d'un développeur non reconnu. Le bouton **Install anyway** est derrière *More details*.
4. Touchez **Open**. Sur un téléphone la calculatrice remplit l'écran, comme le fait Droid48.

Ces deux avertissements portent sur *d'où vient le fichier*, pas sur ce qu'il contient — et le second est inévitable pour toute application qui n'est pas distribuée par le Play Store.

Android **8.0 ou plus récent**, et **arm64-v8a**, c'est-à-dire tout téléphone vendu depuis 2016 environ. Un téléphone 32 bits, ou une image d'émulateur compilée pour x86_64, refusera de l'installer.

## Ensuite donnez-lui une ROM, et c'est la seule étape pénible

La seule chose absente du téléchargement est l'**image de la ROM** — le micrologiciel de la calculatrice elle-même, le code que HP a gravé dans la vraie machine. Agape48 ne l'inclut pas, parce que c'est le code de HP et pas le nôtre.

C'est cependant un téléchargement gratuit, et ce n'est pas un clin d'œil : **HP a donné l'autorisation de télécharger ces ROM au milieu de l'an 2000**, et elles sont sur hpcalc.org depuis. C'est l'archive dont se sert tout le loisir HP.

### En obtenir une

1. Allez sur **<https://www.hpcalc.org/hp48/pc/emulators/>**.
2. Cette page, c'est surtout des émulateurs, pas des ROM, et les images sont très loin en bas. Ne faites pas défiler — appuyez sur **Ctrl-F** (**⌘-F** sur un Mac) et cherchez dans la page **`HP 48GX Revision`**. Cela vous dépose dessus.
3. Il y en a onze, et n'importe laquelle marche. Si vous voulez celle qui vous permettra de ne plus y penser, prenez **`gxrom-r.zip`** — la dernière révision de la 48GX, qui est la calculatrice d'après laquelle cette façade est dessinée. 314 Ko.
4. **Décompressez-la, et faites-le avant de partir chercher le fichier.** Dedans il y a un seul fichier appelé **`gxrom-r`** sans extension, de 524 288 octets. Ce fichier *est* la ROM : rien à convertir, rien à déballer de plus. Un fichier encore à l'intérieur du `.zip` est invisible pour Agape48 et pour n'importe quelle fenêtre d'ouverture, ce qui est la raison la plus probable de « je l'ai téléchargée et le programme ne la trouve pas ».
5. Si ensuite vous ne retrouvez pas le fichier décompressé, **triez votre dossier Téléchargements par date et regardez la chose la plus ancienne qui s'y trouve.** Ces ROM portent leurs dates d'origine du début des années 2000, donc le fichier le plus récent que vous ayez est celui qui a l'air d'avoir vingt-cinq ans.

Les autres, si vous êtes curieux plutôt que pressé : `gxrom-k` à `gxrom-r` sont les révisions K, L, M, P et R de la 48GX, et `sxrom-a` à `sxrom-j` sont la 48SX plus ancienne. Plus tard vaut généralement mieux — la R a corrigé des bugs que la K avait — mais une ROM de 48SX transforme Agape48 en 48SX, ce qui est justement l'intérêt d'avoir le choix.

### La mettre là où la calculatrice regarde

Les deux façons marchent, et aucune n'a besoin de l'autre :

**La façon sûre — renommez et déposez.** Renommez le fichier exactement en **`rom`**, sans extension, et mettez-le dans le dossier de la calculatrice elle-même :

| | |
|---|---|
| Windows | `%LOCALAPPDATA%\Agape48\Agape48` |
| Linux | `~/.local/share/Agape48/Agape48` |
| Android | `Android/media/br.gbmaizol.agape48/Agape48 calculators` |

Sur Android c'est de loin la plus facile des deux : le dossier est visible dans n'importe quel gestionnaire de fichiers et par un câble USB et ne demande aucune autorisation, donc vous branchez le téléphone sur un ordinateur et vous y déposez le fichier.

**La façon rapide — désignez le fichier à l'application.** Lancez Agape48 et cliquez sur le **48GX** souligné dans le coin supérieur droit de la calculatrice — c'est le menu, et il n'y a pas d'autre entrée. Ensuite **Settings → HP 48 ROM**, et choisissez le fichier là où il est déjà. La calculatrice démarre à l'instant où vous appuyez sur Open, et le chemin est retenu, donc vous ne le refaites jamais.

Deux plis, tous deux au sujet de la fenêtre d'ouverture. Elle ne peut pas regarder dans un `.zip`, donc décompressez d'abord. Et elle liste les noms qui ressemblent à une ROM, ce qui inclut `gxrom-r` et `sxrom-a` — mais si vous avez renommé le fichier autrement et que vous ne le voyez pas, passez le filtre de la fenêtre sur **All files**.

### Quand le faire

Quand vous voulez. Installez d'abord et lancez-la, et s'il n'y a pas encore de ROM elle le dit et **écrit le dossier exact où elle a regardé** — donc l'ordre honnête est : installez, lancez une fois, lisez le dossier à l'écran, déposez le fichier, relancez.

### Le premier écran, pour qu'il ne vous inquiète pas

Avec une ROM neuve la calculatrice demande **`Try To Recover Memory?`**, parce que sa mémoire n'a jamais été écrite. C'est la vraie HP 48 qui demande, exactement comme une neuve le ferait. Appuyez sur la **plus à droite des six touches blanches** du haut — c'est **NO** — et vous avez une calculatrice propre.

Où vit ce dossier vous appartient et se déplace quand vous voulez, et c'est le même réglage qui laisse une calculatrice habiter Dropbox et s'ouvrir depuis les deux machines.

## Logiciel libre

Agape48 est un logiciel libre sous la **Licence publique générale GNU, version 3**, et il doit l'être : il repose sur le cœur Saturn `x48` écrit par Eddie C. Dost et sur les années de travail que Droid48 y a mises, tous deux sous GPL. Les fichiers de Dost disent « version 2, ou toute version ultérieure », et la version 3 est l'ultérieure que ce projet prend — la version qui vous défend aussi contre les revendications de brevet et contre quiconque livrerait ceci sur un appareil que vous n'avez pas le droit de modifier. Donc la source est là, en entier, et tout ce qui est construit dessus reste libre de la même façon. Le texte complet est dans `LICENSE`.

## Pour les programmeurs

`Readme_Programmers.md` — la compilation, la structure, le budget de taille, la couture entre l'interface Qt et le cœur en C, et la seule règle inhabituelle : **tout ce qui est écrit à la main dans ce projet est en espéranto.** Les commentaires, les pull requests qui sont fusionnées, les rapports de bugs, les tickets. Si vous ne le lisez pas encore, cette page plaide pour deux mois de soirées.
