# Aplicação multimídia com H.264 e PCM

A aplicação em C amplia a atividade anterior. Ela reproduz o vídeo de teste após codificá-lo e decodificá-lo em H.264, mantém a comparação de vídeo lado a lado e acrescenta áudio PCM.

O programa executa A e depois B. Cada configuração dura aproximadamente 10 segundos. Durante cada execução, áudio e vídeo pertencem à mesma pipeline e usam o mesmo relógio do GStreamer.

| Configuração | Taxa | Formato PCM | Canais | Arquivo gerado |
|---|---:|---|---:|---|
| A | 48.000 Hz | `S16LE`, inteiro com sinal de 16 bits | 2, estéreo | `pcm_a.wav` |
| B | 8.000 Hz | `U8`, inteiro sem sinal de 8 bits | 1, mono | `pcm_b.wav` |

As fontes são `videotestsrc` e `audiotestsrc`. Não é necessário baixar um vídeo ou um áudio para executar a aplicação.

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
./atividade
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
./atividade
```

Antes da demonstração, confira os elementos novos:

```sh
gst-inspect-1.0 x264enc
gst-inspect-1.0 h264parse
gst-inspect-1.0 avdec_h264
gst-inspect-1.0 wavenc
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

O resultado esperado é A com 48.000 Hz, 16 bits e dois canais, B com 8.000 Hz, 8 bits e um canal, e aproximadamente 10 segundos em ambos. A contém 1.920.000 bytes de dados PCM; B contém 80.000. O tamanho total do WAV inclui também o cabeçalho e metadados.

## Abrir os diagramas e preparar a apresentação

Abra [pipelines.drawio](diagramas/pipelines.drawio) em [diagrams.net](https://app.diagrams.net/) com **Arquivo → Abrir de → Dispositivo**. O arquivo contém três páginas, para vídeo, áudio A e áudio B.

As mesmas arquiteturas também estão disponíveis em SVG:

- [Vídeo H.264](diagramas/video.svg)
- [Áudio A](diagramas/audio_a.svg)
- [Áudio B](diagramas/audio_b.svg)

Leia [EXPLICACAO.md](EXPLICACAO.md) para relacionar cada elemento aos conceitos da disciplina e aos exemplos oficiais usados no código.

Na demonstração, execute `./atividade`, acompanhe a configuração indicada no terminal e compare os dois áudios. Mostre as caps em `configuracoes[]`, o fluxo H.264 em `video` e as três páginas do diagrama. Explique a diferença de taxa de amostragem, profundidade e canais usando os WAVs gerados.

## Verificação realizada

O código foi compilado em Linux com `-std=c11 -Wall -Wextra -Werror`. A aplicação concluiu A e B com código de saída zero. Seus WAVs foram conferidos quanto a formato, taxa, canais, duração e quantidade de dados, e o fluxo em execução negociou `video/x-h264`.

O ambiente de verificação não oferece dispositivos gráficos e de som. A exibição e a escuta no macOS precisam ser conferidas no computador da demonstração.
