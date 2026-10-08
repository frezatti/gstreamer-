# Aplicação multimídia com H.264 e PCM

A aplicação em C recebe um arquivo local com vídeo e áudio. Ela usa `uridecodebin` e o callback `pad-added`, seguindo o Tutorial 3, para conectar os fluxos aos ramos de processamento quando os pads aparecem.

O vídeo passa por codificação e decodificação H.264 e aparece lado a lado: referência de 640 × 480 a 30 FPS e versão em cinza de 320 × 240 a 10 FPS. A trilha de áudio do arquivo é convertida para PCM.

O programa reproduz o arquivo inteiro em A e depois abre o mesmo arquivo em B. Durante cada execução, áudio e vídeo pertencem à mesma pipeline e usam o mesmo relógio do GStreamer. A duração depende do arquivo escolhido.

| Configuração | Taxa | Formato PCM | Canais | Arquivo gerado |
|---|---:|---|---:|---|
| A | 48.000 Hz | `S16LE`, inteiro com sinal de 16 bits | 2, estéreo | `pcm_a.wav` |
| B | 8.000 Hz | `U8`, inteiro sem sinal de 8 bits | 1, mono | `pcm_b.wav` |

Incluímos [exemplo.mp4](exemplo.mp4), com cerca de cinco segundos, vídeo H.264 e áudio AAC. Esse arquivo foi gerado pelo GStreamer apenas para facilitar a demonstração. Você pode substituí-lo por um vídeo seu que contenha áudio e cujos codecs estejam disponíveis na instalação.

O áudio AAC do exemplo é decodificado para `audio/x-raw` antes da conversão PCM. A saída de áudio é PCM em WAV, sem compressão.

## Compilar e executar

Entre nesta pasta antes de compilar. Os WAVs são gerados na pasta em que você executa o programa. Uma nova execução substitui os WAVs anteriores.

### macOS com o SDK oficial

Use a instalação de GStreamer da atividade anterior. Se estiver instalando do zero, instale os pacotes de runtime e de desenvolvimento com os plugins completos, seguindo a [documentação oficial para macOS](https://gstreamer.freedesktop.org/documentation/installing/on-mac-osx.html).

```sh
export PATH="/Library/Frameworks/GStreamer.framework/Versions/1.0/bin:$PATH"
export PKG_CONFIG_PATH="/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig"
clang -std=c11 -Wall -Wextra main.c -o atividade \
  $(pkg-config --cflags --libs gstreamer-1.0) \
  -Wl,-rpath,/Library/Frameworks/GStreamer.framework/Versions/1.0/lib
./atividade exemplo.mp4
```

O `main()` preserva o trecho `__APPLE__` e a chamada a `gst_macos_main()` dos tutoriais. Essa função é fornecida pelo GStreamer.

Se sua instalação usa Homebrew, use o `pkg-config` dessa instalação e compile com `clang -std=c11 -Wall -Wextra main.c -o atividade $(pkg-config --cflags --libs gstreamer-1.0)`. A [fórmula atual do GStreamer](https://formulae.brew.sh/formula/gstreamer) inclui os plugins. Não misture os caminhos do Homebrew com os do SDK oficial.

### Linux, Ubuntu ou Linux Mint

```sh
sudo apt install build-essential pkg-config libgstreamer1.0-dev \
  gstreamer1.0-tools gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav
gcc -std=c11 -Wall -Wextra main.c -o atividade \
  $(pkg-config --cflags --libs gstreamer-1.0)
./atividade exemplo.mp4
```

### Windows com MSVC x64

Instale os [Build Tools do Visual Studio](https://visualstudio.microsoft.com/downloads/) ou o Visual Studio, com a carga de trabalho **Desenvolvimento para desktop com C++**. Use Visual Studio 2022 ou mais recente.

Na [página oficial do GStreamer](https://gstreamer.freedesktop.org/download/), baixe os dois instaladores **MSVC x86_64** da mesma versão: Runtime e Development. Instale ambos, selecionando os plugins completos. Use os pacotes MSVC para este procedimento.

Abra **x64 Native Tools Command Prompt** do Visual Studio e entre na pasta `atividade_mpeg4_pcm` do repositório. Estes comandos usam `C:\gstreamer\1.0\msvc_x86_64` como exemplo. Se escolheu outra pasta na instalação, ajuste `GST_DIR`:

```bat
cd /d "C:\caminho\gstreamer-\atividade_mpeg4_pcm"
set "GST_DIR=C:\gstreamer\1.0\msvc_x86_64"
set "PATH=%GST_DIR%\bin;%PATH%"
compilar_windows.bat
atividade.exe exemplo.mp4
```

Para um vídeo seu, execute `atividade.exe "C:\Users\seu_nome\Videos\meu video.mp4"`. O arquivo precisa conter vídeo e áudio. A aplicação produz `pcm_a.wav` e `pcm_b.wav` na pasta atual.

Confira os plugins necessários antes da demonstração:

```bat
gst-inspect-1.0 uridecodebin
gst-inspect-1.0 x264enc
gst-inspect-1.0 h264parse
gst-inspect-1.0 avdec_h264
gst-inspect-1.0 wavenc
```

Se `cl` não for encontrado, abra o terminal do Visual Studio indicado acima. Se faltar uma DLL do GStreamer, confira o comando que acrescenta `%GST_DIR%\bin` ao `PATH`. Se um elemento não for encontrado, revise a instalação completa do Runtime.

O `main()` usa `tutorial_main()` diretamente no Windows. O trecho com `gst_macos_main()` só entra na compilação para macOS. O procedimento acima está baseado na [documentação do GStreamer para Windows](https://gstreamer.freedesktop.org/documentation/installing/on-windows.html) e no [uso das ferramentas MSVC no terminal](https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line). A compilação nativa no Windows ainda precisa ser conferida no computador do grupo.

Para usar seu próprio arquivo no macOS ou Linux, passe seu caminho entre aspas:

```sh
./atividade "/Users/seu_nome/Movies/meu video.mp4"
```

São aceitos caminhos relativos ou absolutos. O programa usa a primeira trilha de vídeo e a primeira de áudio que forem conectadas. Arquivos sem uma dessas mídias produzem um erro. A extensão `.mp4` é um exemplo; o suporte real depende dos demuxers e decoders instalados.

Antes da demonstração, confira os elementos novos:

```sh
gst-inspect-1.0 x264enc
gst-inspect-1.0 h264parse
gst-inspect-1.0 avdec_h264
gst-inspect-1.0 wavenc
gst-inspect-1.0 uridecodebin
```

Se um elemento não existir, complete a instalação dos plugins. `x264enc` pertence a Ugly, `h264parse` a Bad, `avdec_h264` a Libav e `wavenc` a Good.

## Conferir os resultados

Espere o programa imprimir `Fim da configuracao B.` antes de abrir os WAVs. O encerramento por EOS permite finalizar seus cabeçalhos.

Ouça `pcm_a.wav` e `pcm_b.wav` no mesmo volume. Para reproduzir pelos próprios elementos do GStreamer, execute os comandos separadamente:

```sh
gst-launch-1.0 filesrc location=pcm_a.wav ! wavparse ! audioconvert ! audioresample ! autoaudiosink
gst-launch-1.0 filesrc location=pcm_b.wav ! wavparse ! audioconvert ! audioresample ! autoaudiosink
```

O script opcional usa apenas Python 3 e confere os WAVs gerados pela aplicação:

```sh
python3 verificar_pcm.py
```

O resultado esperado é A com 48.000 Hz, 16 bits e dois canais, B com 8.000 Hz, 8 bits e um canal, e durações praticamente iguais. Para `exemplo.mp4`, ambos têm cerca de 5,02 segundos de áudio. A usa aproximadamente 24 vezes mais dados PCM por segundo. O tamanho total do WAV inclui também o cabeçalho e metadados.

O script compara os formatos e a duração dos dois WAVs. Para confirmar que todo o áudio foi convertido, compare também essa duração com a trilha de áudio do arquivo de entrada.

## Abrir os diagramas e preparar a apresentação

Os diagramas editáveis para [Excalidraw](https://excalidraw.com/) estão nestes arquivos:

- [Vídeo H.264](diagramas/video.excalidraw)
- [Áudio PCM A](diagramas/audio_a.excalidraw)
- [Áudio PCM B](diagramas/audio_b.excalidraw)

Importe o arquivo `.excalidraw` correspondente no Excalidraw para editar os elementos, as propriedades e as conexões. Os três diagramas representam os ramos da mesma pipeline: vídeo e áudio compartilham uma fonte `uridecodebin` em cada execução de A ou B. As conexões dinâmicas do sinal `pad-added` aparecem tracejadas.

Abra [pipelines.drawio](diagramas/pipelines.drawio) em [diagrams.net](https://app.diagrams.net/) com **Arquivo → Abrir de → Dispositivo**. O arquivo contém três páginas, para vídeo, áudio A e áudio B.

As mesmas arquiteturas também estão disponíveis em SVG:

- [Vídeo H.264](diagramas/video.svg)
- [Áudio A](diagramas/audio_a.svg)
- [Áudio B](diagramas/audio_b.svg)

Leia [EXPLICACAO.md](EXPLICACAO.md) para relacionar cada elemento aos conceitos da disciplina e aos exemplos oficiais usados no código. O [guia linha a linha](GUIA_LINHA_A_LINHA.md) acompanha as 258 linhas de `main.c`, explica a ordem de execução, as decisões e a origem dos trechos nos tutoriais.

Na demonstração, execute `./atividade exemplo.mp4`, acompanhe a configuração e as mensagens `Pad dinamico` no terminal e compare os dois áudios. Mostre `pad_added_handler()`, as caps em `configuracoes[]`, o fluxo H.264 em `video` e as três páginas do diagrama. Explique a diferença de taxa de amostragem, profundidade e canais usando os WAVs gerados.

A parte dinâmica é a ligação da fonte às filas `entrada_video` e `entrada_audio`. Os elementos dos ramos de processamento já estão preparados quando a reprodução começa.

## Verificação realizada

O código foi compilado em Linux com `-std=c11 -Wall -Wextra -Werror`. A aplicação concluiu A e B usando o arquivo fornecido. Os WAVs foram conferidos quanto a formato, taxa, canais, duração e quantidade de dados. Também foram verificados caminhos com espaços, outra codificação de entrada, áudio silencioso, múltiplas trilhas e erros de arquivo inexistente, inválido ou sem uma das trilhas.

Uma captura com a saída de vídeo direcionada para PNG confirmou a composição de 960 × 480, com a referência colorida e a versão menor em cinza. Nessa execução, o encoder negociou `video/x-h264` e o ramo processado negociou `GRAY8`, 320 × 240 e 10 FPS.

O ambiente de verificação não oferece dispositivos gráficos e de som. A exibição e a escuta no macOS precisam ser conferidas no computador da demonstração.
