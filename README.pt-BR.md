# Agape48

[English](README.md) · [Dansk](README.da.md) · [Deutsch](README.de.md) · [Español](README.es.md) · [Esperanto](README.eo.md) · [Français](README.fr.md) · **Português**

**O HP 48 que você já ama, no computador e no telefone que você usa de verdade — e ele leva a sua calculadora junto.**

«HP», em português, se fala *agá-pê*, que é o grego ἀγάπη: amor. Agape48 tem o nome do jeito como as pessoas falam desta calculadora.

---

## Se você nunca usou um HP, leia esta parte primeiro

Quase toda calculadora que você já pegou na vida funciona assim:

```
( 3 + 4 ) × 5 =
```

Você teve que digitar dois parênteses para dizer o que queria, e só descobriu lá no fim se era isso mesmo. Erre um deles e a resposta sai errada, caladinha.

Um HP 48 funciona assim:

```
3 ENTER 4 + 5 ×
```

Diga o que você tem, depois diga o que fazer com aquilo. É só isso que RPN é. **Você nunca mais digita um parêntese**, e a calculadora mostra cada resultado intermediário na hora em que ele acontece — depois de `4 +` dá para ver o `7` ali parado, então o erro aparece no momento em que você comete, em vez de no fim.

Lê-se estranho por uns vinte minutos e depois lê-se como aritmética. Três coisas acontecem em seguida:

- **Você para de contar teclas** — RPN normalmente pede menos, e nunca mais.
- **Você para de recomeçar.** Tudo o que você digitou fica numa pilha, visível, na sua frente. Estragou o último número? É uma tecla para jogar fora, não um recomeço.
- **Você para de confiar numa linha só.** Uma conta longa numa calculadora comum é um número e uma esperança. Aqui são quatro números que você pode olhar.

Se você quer a versão curta: digite os números, aperte `ENTER` entre eles, depois aperte a operação. Isso já basta para fazer trabalho de verdade no primeiro dia.

## Por que esta

Emuladores de HP 48 existem há trinta anos, e são bons — o Agape48 está construído sobre o núcleo de um deles. O que nunca existiu foi *um* deles que se comporte do mesmo jeito no seu notebook e no seu telefone, e que passe a calculadora de um para o outro.

| | Windows | Linux | Android | lê arquivos de objeto do HP 48 | escreve esses arquivos |
| --- | :---: | :---: | :---: | :---: | :---: |
| **Agape48** | **sim** | **sim** | **sim** | **sim** | **sim** |
| Emu48 | sim | — | — | sim | sim |
| Droid48 | — | — | sim | sim | — |
| x48 | — | sim | — | — | — |

Esse último par de colunas é o que importa na prática: um arquivo de objeto do HP 48 é o jeito como um programa ou uma matriz anda entre todos esses e uma calculadora de verdade, e até agora nenhum programa sozinho lia e escrevia esses arquivos em todo lugar. No Android, nada fazia as duas coisas.

## A sua calculadora, não uma cópia dela

A memória da calculadora é **uma pasta que você escolhe**. Ponha essa pasta no Dropbox, no OneDrive, no Syncthing, num pen drive — qualquer coisa que sincronize arquivos — e a calculadora que você estava usando no notebook é a calculadora que abre no telefone, com as suas variáveis, os seus programas e a sua pilha exatamente onde você deixou.

Dois detalhes honestos, porque esta é a parte que os outros emuladores deixam você descobrir sozinho:

- **Uma de cada vez.** A memória de uma calculadora é um monte com ponteiros para dentro de si mesma; duas máquinas editando ao mesmo tempo não tem quem consiga mesclar. Então o Agape48 põe uma marca na pasta dizendo qual máquina está com ela aberta, avisa quando outra está, e **fala isso em vez de escolher um vencedor caladinho**. É o mesmo acordo que um cofre de senhas te dá, e pelo mesmo motivo.
- **É mesmo só arquivo.** Nenhuma conta, nenhuma nuvem nossa, nada ligando para casa. O seu programa de sincronização já era bom nisso; o Agape48 não tenta substituir.

Você pode manter **várias calculadoras na mesma pasta** — uma para o trabalho, uma para a prova, uma que você ainda não sabe bem — e trocar em *Open another calculator…*. Elas dividem a ROM, então a segunda custa quase nada.

## O seu teclado, as suas teclas

O teclado físico inteiro pode ser remapeado, e o jeito de descobrir o que uma tecla faz é **passar o mouse por cima dela**: a calculadora diz quais teclas do seu teclado apertam aquela.

- **Clique com o botão direito em qualquer tecla** da face para mudar. Aperte a tecla que você quer. Pronto.
- Dois nomes numa tecla da calculadora quer dizer que duas teclas do seu teclado chegam nela, que costuma ser o que você quer — `Enter` e o `Enter` do teclado numérico apertam os dois o `ENTER`.
- No telefone não existe botão direito, então *Customize keyboard…* no menu liga a mesma coisa para um toque simples.

Nada é fixo. O mapa inteiro mora num arquivo de texto pequeno **ao lado da sua ROM**, na mesma pasta da memória da calculadora — então os seus atalhos viajam com a calculadora em vez de ficar para trás numa máquina só.

## Tão rápida quanto você quiser, inclusive «nada»

Um HP 48 de verdade roda a uns 2 MHz em silício de 1990, e existe uma chave para isso: **Slow down to real calculator speed** deixa ele tão lento quanto o da gaveta, de propósito, porque às vezes a graça é justamente a calculadora de que você lembra. Fora isso, um controle deslizante leva a velocidade até o limite da sua máquina, que é bem mais rápido que 1990 — útil na primeira vez que você roda um programa que antes dava tempo de tomar um café.

## Botando coisas para dentro e para fora

- **Copiar e colar** com o resto do computador, pela área de transferência do sistema.
- **Importar e exportar arquivos de objeto do HP 48** — o formato `HPHP48-` — nos dois sentidos, que é como um programa vai para um 48 de verdade, para o Emu48, para o Droid48, ou volta. Conferido byte a byte contra o Emu48, com uma biblioteca de terceiros como controle: de 1206 bytes, 1205 voltaram idênticos, e o que difere é o byte que *diz qual máquina escreveu o arquivo*, que é para diferir mesmo.
- **Save memory now** quando você quiser, e automaticamente quando o aplicativo vai para segundo plano.

## Autossuficiente, e fora do caminho

Um arquivo por plataforma, com a própria cópia do Qt dentro, então não tem nenhum runtime para instalar antes. A janela muda de tamanho livremente e mantém as proporções da calculadora; você pode deixar uma tira fina ao lado do seu trabalho ou encher a tela com ela. Os tamanhos de texto são ajustáveis, porque uma calculadora que você deixa aberta o dia inteiro é uma calculadora que você precisa conseguir ler.

## Instalando

Um download por máquina, nenhum runtime para buscar antes, e nenhuma conta para criar.

**Os três estão na [página de versões](https://github.com/gbmaizol/Agape48/releases/latest)**, com um sha256 ao lado de cada um se você quiser conferir o que baixou.

Os avisos citados abaixo aparecem no idioma do seu sistema. Eles estão aqui em inglês porque é assim que o Windows e o Android escrevem numa instalação em inglês.

### Windows

1. Baixe **[`Agape48-0.9.3-windows-x64-setup.exe`](https://github.com/gbmaizol/Agape48/releases/latest)** (17,4 MB).
2. **O Windows vai te barrar**: *«Windows protected your PC»*. Clique em **More info**, depois em **Run anyway**. Essa mensagem não é aviso de vírus — é o Windows dizendo que o instalador não carrega certificado de assinatura de código, que é uma compra e não uma etapa de compilação. Nada no download está empacotado de forma oculta nem ofuscado, e cada byte de fonte que entrou nele está neste repositório.
3. **Next**, **Next**, **Install**. Vai para `Program Files` e cria um item no menu Iniciar.
4. Abra pelo menu Iniciar. Desinstala como qualquer outro programa, em *Apps & features*.

### Linux

Sem root, nada em `/opt`, e nada para adicionar ao seu gerenciador de pacotes. Pegue **[`Agape48-0.9.3-linux-x86_64.tar.gz`](https://github.com/gbmaizol/Agape48/releases/latest)**, depois:

```sh
tar xzf Agape48-0.9.3-linux-x86_64.tar.gz
./Agape48-0.9.3-linux-x86_64/install.sh
```

Não tem etapa de `chmod`: o `tar` mantém o bit de execução, então o `install.sh` simplesmente roda. (Se você descompactou com um arquivador gráfico que perdeu as permissões, `sh install.sh` funciona do mesmo jeito.)

Tudo vai parar em `~/.local/share/agape48`, o item de menu aparece em **Education**, e digitar `agape48` roda o programa se `~/.local/bin` estiver no seu `PATH`. O runtime do Qt viaja dentro do tarball, então não tem pacote de distribuição para caçar e nada para instalar antes — o que também quer dizer que não quebra quando a sua distribuição passar para o próximo Qt. Para remover: `~/.local/share/agape48/uninstall.sh`, que recolhe exatamente o que colocou e não encosta nas suas calculadoras.

O download tem 36,0 MB e descompacta para 98 MB, quase tudo Qt. É compilado para **x86_64** contra a **glibc 2.39**, então Ubuntu 24.04, Mint 22, Debian 13 ou qualquer coisa mais nova. Numa distribuição mais antiga ele para no início com uma linha `GLIBC_2.xx not found`, que parece um crash e não é. Testado no X11.

### Android

1. Baixe **[`Agape48-0.9.3-arm64-v8a.apk`](https://github.com/gbmaizol/Agape48/releases/latest)** (47,6 MB) no telefone e toque nele.
2. **O Android vai recusar na primeira vez** — *«your phone is not allowed to install unknown apps from this source»* — porque o arquivo não veio da Play Store. Toque em **Settings**, libere aquela fonte, e volte. O Android pergunta por *fonte*, então liberar o seu navegador não libera junto o seu gerenciador de arquivos.
3. O Play Protect pode então avisar sobre um aplicativo de um desenvolvedor não reconhecido. O botão **Install anyway** está atrás de *More details*.
4. Toque em **Open**. No telefone a calculadora ocupa a tela inteira, como o Droid48 faz.

Os dois avisos são sobre *de onde o arquivo veio*, não sobre o que tem dentro dele — e o segundo é inevitável para qualquer aplicativo que não seja distribuído pela Play Store.

Android **8.0 ou mais novo**, e **arm64-v8a**, que é todo telefone vendido desde mais ou menos 2016. Um telefone de 32 bits, ou uma imagem de emulador compilada para x86_64, vai se recusar a instalar.

## Depois dê uma ROM para ela, e essa é a única etapa chata

A única coisa que não vem no download é a **imagem da ROM** — o firmware da própria calculadora, o código que a HP gravou na máquina de verdade. O Agape48 não inclui essa imagem, porque o código é da HP e não nosso.

Só que é um download gratuito, e isso não é piscadela: **a HP deu permissão para essas ROMs serem baixadas em meados de 2000**, e elas estão no hpcalc.org desde então. É o acervo que o hobby de HP inteiro usa.

### Conseguindo uma

1. Vá em **<https://www.hpcalc.org/hp48/pc/emulators/>**.
2. Aquela página é quase toda de emuladores, não de ROMs, e as imagens estão bem lá embaixo. Não role a página — aperte **Ctrl-F** (**⌘-F** no Mac) e procure por **`HP 48GX Revision`**. Isso cai direto nelas.
3. São onze, e qualquer uma serve. Se você quer aquela para parar de pensar no assunto, pegue **`gxrom-r.zip`** — a última revisão do 48GX, que é a calculadora a partir da qual esta face foi desenhada. 314 KB.
4. **Descompacte, e faça isso antes de sair procurando o arquivo.** Dentro tem um único arquivo chamado **`gxrom-r`**, sem extensão, com 524.288 bytes. Esse arquivo *é* a ROM: nada para converter, nada para desempacotar de novo. Um arquivo ainda dentro do `.zip` é invisível para o Agape48 e para qualquer janela de abrir arquivo, que é o motivo mais provável de «baixei e o programa não acha».
5. Se depois você não achar o arquivo descompactado, **ordene a sua pasta de Downloads por data e olhe a coisa mais velha que estiver lá.** Essas ROMs carregam as datas originais do começo dos anos 2000, então o arquivo mais novo que você tem é o que parece ter vinte e cinco anos.

As outras, se você está curioso e não com pressa: `gxrom-k` até `gxrom-r` são as revisões K, L, M, P e R do 48GX, e `sxrom-a` até `sxrom-j` são o 48SX mais antigo. Mais tarde costuma ser melhor — a R corrigiu bugs que a K tinha — mas uma ROM de 48SX transforma o Agape48 num 48SX, que é justamente a graça de ter a escolha.

### Pondo onde a calculadora procura

Os dois jeitos funcionam, e nenhum precisa do outro:

**O jeito certeiro — renomeie e solte.** Renomeie o arquivo para exatamente **`rom`**, sem extensão, e ponha na pasta da própria calculadora:

| | |
|---|---|
| Windows | `%LOCALAPPDATA%\Agape48\Agape48` |
| Linux | `~/.local/share/Agape48/Agape48` |
| Android | `Android/media/br.gbmaizol.agape48/Agape48 calculators` |

No Android esse é de longe o mais fácil dos dois: a pasta aparece em qualquer gerenciador de arquivos e pelo cabo USB e não precisa de permissão nenhuma, então você liga o telefone num computador e solta o arquivo lá.

**O jeito rápido — aponte o aplicativo para ele.** Abra o Agape48 e clique no **48GX** sublinhado no canto superior direito da calculadora — aquilo é o menu, e não existe outra entrada. Depois **Settings → HP 48 ROM**, e escolha o arquivo onde ele já está. Escolher só preenche o campo: **Save** é o que carrega, e a calculadora passa para aquela ROM na mesma janela, sem reiniciar. Se o arquivo não for uma ROM de HP 48, o caminho fica vermelho e a linha abaixo dele diz por quê, e Save continua cinza até ali haver uma ROM. O caminho fica guardado, então você nunca mais faz isso.

Duas pegadinhas, as duas sobre a janela de abrir arquivo. Ela não consegue olhar dentro de um `.zip`, então descompacte antes. E ela lista nomes com cara de ROM, o que inclui `gxrom-r` e `sxrom-a` — mas se você renomeou o arquivo para outra coisa e não está vendo, mude o filtro da janela para **All files**.

### Quando fazer isso

Quando você quiser. Instale primeiro e abra, e se ainda não tiver ROM ele avisa e **escreve a pasta exata onde procurou** — então a ordem honesta é: instale, abra uma vez, leia a pasta na tela, solte o arquivo lá, abra de novo.

### A primeira tela, para ela não te assustar

Com uma ROM nova a calculadora pergunta **`Try To Recover Memory?`**, porque a memória dela nunca foi escrita. É o HP 48 de verdade perguntando, exatamente como um novo faria. Aperte a **mais à direita das seis teclas em branco** da fileira de cima — aquela é o **NO** — e você tem uma calculadora limpa.

Onde essa pasta fica é coisa sua e dá para mudar a qualquer momento, e é a mesma configuração que deixa uma calculadora morar no Dropbox e ser aberta das duas máquinas.

## Software livre

O Agape48 é software livre sob a **Licença Pública Geral GNU, versão 3**, e tem que ser: ele está sobre o núcleo Saturn `x48` escrito por Eddie C. Dost e sobre os anos de trabalho que o Droid48 pôs nele, os dois sob a GPL. Os arquivos de Dost dizem «versão 2, ou qualquer versão posterior», e a versão 3 é a posterior que este projeto pega — a versão que também te defende de reivindicações de patente e de quem embarque isto num aparelho que você não tem direito de mexer. Então o fonte está aqui, inteiro, e tudo o que for construído sobre ele continua livre do mesmo jeito. O texto completo está em `LICENSE`.

## Para programadores

`Readme_Programmers.md` — a compilação, o layout, o orçamento de tamanho, a costura entre a interface em Qt e o núcleo em C, e a única regra fora do comum: **tudo escrito à mão neste projeto está em esperanto.** Os comentários, os pull requests que entram, os relatos de bug, as issues. Se você ainda não lê esperanto, aquela página é o argumento a favor de dois meses de noites.
