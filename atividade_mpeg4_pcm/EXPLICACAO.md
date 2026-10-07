# Funcionamento das pipelines

Em cada execução, `gst_parse_launch()` constrói uma pipeline com um fluxo de vídeo e um de áudio. Os fluxos têm saídas próprias, mas compartilham o relógio, os estados e o Bus da pipeline. Não é necessário usar um muxer para reproduzir duas mídias ao mesmo tempo. Um muxer seria necessário para reunir os fluxos em um único arquivo.

O `for` executa a configuração A e, depois que ela termina, executa B. Ele percorre as duas entradas de `configuracoes[]`. Dentro de cada execução, áudio e vídeo funcionam simultaneamente nas threads do GStreamer. As linhas entre aspas em C formam uma única descrição, e `g_strdup_printf()` insere nela o fluxo de vídeo, as caps escolhidas e o nome do WAV.

## Vídeo H.264

`videotestsrc` gera uma bola vermelha sobre um fundo azul. A fonte fornece 300 quadros em `I420`, com resolução de 640 × 480 a 30 FPS. Esse formato é aceito pelo encoder.

`x264enc` transforma o vídeo bruto em H.264, também chamado MPEG-4 AVC ou MPEG-4 Parte 10. O fluxo entre encoder, parser e decoder é efetivamente `video/x-h264`. Um arquivo `.mp4` é um contêiner e não é exigido para que esse fluxo use H.264.

`bitrate=1000` estabelece uma taxa alvo de 1.000 kbit/s para o vídeo comprimido. `speed-preset=ultrafast` prioriza a velocidade de codificação na demonstração. `tune=zerolatency` reduz o acúmulo de quadros no encoder e ajuda a evitar espera excessiva entre os fluxos de áudio e vídeo. Essa escolha tem um custo de eficiência de compressão.

`h264parse` organiza e interpreta a estrutura do fluxo H.264. Ele não reconstrói os quadros. `avdec_h264` faz a decodificação, recuperando vídeo bruto que os filtros e a saída conseguem utilizar.

Após a decodificação, `videoconvert` fornece RGB e `tee name=v` envia os mesmos quadros aos dois ramos da comparação. O ramo esquerdo passa por uma `queue` e entra em `mix.sink_0`. O direito usa `queue`, `videoscale`, `videorate` e `videoconvert`, com caps de 320 × 240, 10 FPS e GRAY8.

`compositor` coloca a referência à esquerda e a versão processada à direita. A posição da direita é x=640 e y=120, centralizando sua altura de 240 pixels na imagem de 480 pixels. A composição final é 960 × 480 a 30 FPS. O ramo direito continua fornecendo apenas 10 atualizações diferentes por segundo.

As duas imagens já passaram pelo encoder e pelo decoder. Portanto, a comparação visual mostra as mudanças de resolução, FPS e cor após a decodificação, e não uma comparação entre vídeo sem compressão e vídeo comprimido.

## Áudio PCM

`audiotestsrc` gera uma onda dente de serra de 440 Hz com volume 0,2. Esse sinal tem harmônicas, o que ajuda a comparar a perda de componentes agudas ao reduzir a taxa de amostragem. A frequência do sinal e o volume da fonte são iguais em A e B.

As caps da fonte fixam `F32LE`, 48.000 Hz e dois canais. Assim, ambas as configurações partem da mesma representação. São 100 buffers com 4.800 amostras por canal, totalizando 480.000 amostras por canal e 10 segundos.

`audioconvert` altera a representação das amostras, a profundidade e os canais. `audioresample` altera a taxa de amostragem. As caps seguintes definem o resultado que esses elementos precisam produzir. As caps restringem os formatos negociados; elas não fazem a conversão por conta própria.

| Propriedade | A | B | Conceito |
|---|---|---|---|
| `rate` | 48.000 Hz | 8.000 Hz | Quantidade de amostras por segundo, em cada canal |
| `format` | `S16LE` | `U8` | Inteiro com sinal de 16 bits ou inteiro sem sinal de 8 bits |
| `channels` | 2 | 1 | Dois canais ou um canal |
| `layout` | `interleaved` | `interleaved` | Amostras dos canais intercaladas no buffer |

Após essas caps, `tee name=a` divide o mesmo PCM entre reprodução e arquivo. Cada ramo tem uma `queue` para permitir processamento em uma thread própria.

No ramo de arquivo, `wavenc` escreve o PCM em um contêiner WAV, sem compressão das amostras. Ele aceita `S16LE` e `U8`, os formatos escolhidos. `filesink` grava `pcm_a.wav` ou `pcm_b.wav`.

No ramo de reprodução, os outros `audioconvert` e `audioresample` adaptam o áudio ao dispositivo escolhido por `autoaudiosink`. Essa adaptação não altera os dados do ramo WAV. Se o dispositivo reproduzir B a uma taxa maior, ele não recupera as informações já perdidas na conversão para 8.000 Hz e 8 bits.

## Diferenças que o grupo deve explicar

A taxa de amostragem limita as frequências representáveis. Pelo critério de Nyquist, os limites teóricos são 24.000 Hz em A e 4.000 Hz em B. A reamostragem para B reduz componentes acima da faixa que 8.000 Hz consegue representar. A frequência fundamental de 440 Hz permanece, mas o timbre pode mudar porque a onda dente de serra contém harmônicas.

Uma amostra de 16 bits dispõe de 65.536 níveis possíveis; uma de 8 bits, de 256. A representação de 8 bits tem passos de amplitude maiores e mais erro de quantização. Esses valores descrevem a capacidade do formato. O sinal de teste não usa necessariamente todos os níveis possíveis.

A tem dois canais, B tem um. Neste gerador, os dois canais de origem contêm o mesmo sinal. Portanto, a mudança para mono pode não produzir uma diferença espacial perceptível, embora a quantidade de canais e de dados mude efetivamente.

Para PCM sem compressão, a taxa de dados é `rate × bits por amostra × channels`:

- A: `48000 × 16 × 2 = 1536000 bit/s`, ou 192.000 bytes por segundo.
- B: `8000 × 8 × 1 = 64000 bit/s`, ou 8.000 bytes por segundo.

Nos arquivos de 10 segundos gerados e verificados, A contém 1.920.000 bytes de PCM e B contém 80.000. A usa 24 vezes mais dados. A duração e a frequência fundamental não mudam por causa dessa redução de tamanho.

## Tratamento de erros e encerramento

O programa verifica erros de `gst_parse_launch()`, incluindo elementos ausentes e ligações incompatíveis. Também verifica o retorno de `gst_element_set_state()` ao solicitar `GST_STATE_PLAYING`.

O Bus entrega mensagens dos elementos ao código da aplicação. `gst_bus_timed_pop_filtered()` aguarda `GST_MESSAGE_ERROR` ou `GST_MESSAGE_EOS`. Em um erro, o programa informa o elemento, a descrição e os detalhes disponíveis. Ao receber EOS, finaliza a configuração e libera os recursos antes de começar a seguinte.

`num-buffers` limita as fontes para que os fluxos terminem. O EOS permite que `wavenc` finalize o cabeçalho do WAV. Depois, a aplicação passa a pipeline para `GST_STATE_NULL` e libera as referências. Uma falha retorna código 1; o término normal das duas configurações retorna 0.

## Fontes e exemplos utilizados

Os trechos de inicialização, estado, Bus, tratamento de mensagens e entrada do macOS foram adaptados dos tutoriais oficiais. As descrições completas das nossas pipelines combinam esses exemplos com a documentação dos elementos.

| Referência | Uso nesta implementação |
|---|---|
| [Basic Tutorial 1](https://gstreamer.freedesktop.org/documentation/tutorials/basic/hello-world.html) | `gst_init`, `gst_parse_launch`, execução, limpeza e entrada no macOS |
| [Basic Tutorial 2](https://gstreamer.freedesktop.org/documentation/tutorials/basic/concepts.html) | Verificação de estado e `switch` para ERROR e EOS no Bus |
| [Basic Tutorial 6](https://gstreamer.freedesktop.org/documentation/tutorials/basic/media-formats-and-pad-capabilities.html) | Caps e negociação dos formatos de áudio e vídeo |
| [Basic Tutorial 10](https://gstreamer.freedesktop.org/documentation/tutorials/basic/gstreamer-tools.html) | Descrições textuais, caps filters, `tee name=...`, referências com ponto e `gst-inspect-1.0` |
| [Raw Audio Media Types](https://gstreamer.freedesktop.org/documentation/additional/design/mediatype-audio-raw.html) | Significado e formatos suportados de `format`, `rate`, `channels` e `layout` |
| [audioconvert](https://gstreamer.freedesktop.org/documentation/audioconvert/index.html) | Conversão de representação, profundidade e canais |
| [audioresample](https://gstreamer.freedesktop.org/documentation/audioresample/index.html) | Reamostragem do áudio bruto |
| [audiotestsrc](https://gstreamer.freedesktop.org/documentation/audiotestsrc/index.html) | Fonte gerada e exemplo de onda dente de serra com `tee` |
| [x264enc](https://gstreamer.freedesktop.org/documentation/x264/index.html) | Codificação H.264, formato I420 e parâmetros do encoder |
| [h264parse](https://gstreamer.freedesktop.org/documentation/videoparsersbad/h264parse.html) | Organização do fluxo H.264 |
| [avdec_h264](https://gstreamer.freedesktop.org/documentation/libav/avdec_h264.html) | Decodificação H.264 |
| [wavenc](https://gstreamer.freedesktop.org/documentation/wavenc/index.html) | WAV com amostras PCM, suporte a S16LE e U8 |
| [compositor](https://gstreamer.freedesktop.org/documentation/compositor/index.html) | Comparação visual e posição das entradas |

Escolhemos fontes geradas para que a demonstração não dependa de arquivos externos nem da rede. Mantivemos a construção por descrição textual porque ela já era usada na atividade anterior e permite mostrar a sequência dos elementos diretamente no código.
