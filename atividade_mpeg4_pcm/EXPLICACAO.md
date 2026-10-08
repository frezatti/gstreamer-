# Funcionamento das pipelines

Para acompanhar o arquivo C inteiro, consulte o [guia linha a linha](GUIA_LINHA_A_LINHA.md). Ele explica cada trecho, as decisões da implementação e as referências dos tutoriais.

Em cada execução, `gst_parse_launch()` prepara os ramos de vídeo e áudio. Depois, o programa cria `uridecodebin` com `gst_element_factory_make()` e o adiciona à mesma pipeline. Os fluxos têm saídas próprias, mas compartilham o relógio, os estados e o Bus da pipeline. Não é necessário usar um muxer para reproduzir duas mídias ao mesmo tempo. Um muxer seria necessário para reunir os fluxos em um único arquivo.

O `for` executa a configuração A e, depois que ela termina, executa B. Ele percorre as duas entradas de `configuracoes[]`. Dentro de cada execução, áudio e vídeo funcionam simultaneamente nas threads do GStreamer. As linhas entre aspas em C formam uma única descrição, e `g_strdup_printf()` insere nela o fluxo de vídeo, as caps escolhidas e o nome do WAV.

## Arquivo e ligação dinâmica: Tutorial 3

O argumento `argv[1]` é o caminho do arquivo passado em `./atividade "video.mp4"`. `gst_filename_to_uri()` transforma esse caminho em uma URI `file://`, incluindo o tratamento de espaços e caminhos relativos. `g_object_set()` coloca essa URI na propriedade `uri` de `uridecodebin`.

`uridecodebin` escolhe internamente a fonte de arquivo, o demuxer do contêiner e os decoders necessários. Seus pads de saída aparecem conforme os fluxos são identificados; eles ainda não estão disponíveis quando criamos o elemento.

Por isso, usamos o trecho do Tutorial 3:

```c
g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler), &data);
```

Quando surge um pad, o GStreamer chama `pad_added_handler()`. A estrutura `CustomData` fornece ao callback as filas que recebem os fluxos. O callback consulta as caps do pad e escolhe:

| Tipo do pad | Destino |
|---|---|
| `video/x-raw` | pad `sink` de `entrada_video` |
| `audio/x-raw` | pad `sink` de `entrada_audio` |
| Outros tipos | Ignorados |

`gst_pad_link()` liga o novo pad de saída ao pad de entrada escolhido. Se a fila já recebeu uma trilha, outras trilhas do mesmo tipo são ignoradas. O Tutorial 3 original liga somente áudio; nossa adaptação acrescenta a escolha do ramo de vídeo.

Essa é a parte dinâmica da pipeline: a fonte é conectada durante a descoberta dos fluxos. Os filtros, as divisões com `tee` e as saídas já foram criados antes de `GST_STATE_PLAYING`.

O sinal `no-more-pads` informa que a fonte terminou de criar seus pads. Nosso segundo callback verifica se há vídeo e áudio conectados. Se faltar um deles, publica uma mensagem de erro no Bus, evitando que a aplicação espere por um fluxo ausente.

## Vídeo H.264

O pad de vídeo de `uridecodebin` fornece os quadros decodificados do arquivo. `entrada_video` é uma `queue`, seguida de `videoconvert`, `videoscale` e `videorate`. Esses elementos adaptam a entrada para `I420`, 640 × 480 e 30 FPS, com pixels quadrados (`pixel-aspect-ratio=1/1`). Assim, um arquivo com outra resolução ou FPS também pode alimentar o encoder e a comparação.

`x264enc` transforma o vídeo bruto em H.264, também chamado MPEG-4 AVC ou MPEG-4 Parte 10. O fluxo entre encoder, parser e decoder é efetivamente `video/x-h264`. Um arquivo `.mp4` é um contêiner e não é exigido para que esse fluxo use H.264.

Mantivemos essa etapa H.264 explícita da atividade anterior. Mesmo se o arquivo de entrada tiver outro codec, o ramo implementado utiliza H.264. Se a entrada já for H.264, ela será decodificada por `uridecodebin` e depois passará novamente pelo encoder e pelo decoder usados na demonstração.

`bitrate=1000` estabelece uma taxa alvo de 1.000 kbit/s para o vídeo comprimido. `speed-preset=ultrafast` prioriza a velocidade de codificação na demonstração. `tune=zerolatency` reduz o acúmulo de quadros no encoder e ajuda a evitar espera excessiva entre os fluxos de áudio e vídeo. Essa escolha tem um custo de eficiência de compressão.

`h264parse` organiza e interpreta a estrutura do fluxo H.264. Ele não reconstrói os quadros. `avdec_h264` faz a decodificação, recuperando vídeo bruto que os filtros e a saída conseguem utilizar.

Após a decodificação, `videoconvert` fornece RGB e `tee name=v` envia os mesmos quadros aos dois ramos da comparação. O ramo esquerdo passa por uma `queue` e entra em `mix.sink_0`. O direito usa `queue`, `videoscale`, `videorate` e `videoconvert`, com caps de 320 × 240, 10 FPS e GRAY8.

`compositor` coloca a referência à esquerda e a versão processada à direita. A posição da direita é x=640 e y=120, centralizando sua altura de 240 pixels na imagem de 480 pixels. A composição final é 960 × 480 a 30 FPS. O ramo direito continua fornecendo apenas 10 atualizações diferentes por segundo.

As duas imagens já passaram pelo encoder e pelo decoder. Portanto, a comparação visual mostra as mudanças de resolução, FPS e cor após a decodificação, e não uma comparação entre vídeo sem compressão e vídeo comprimido.

## Áudio PCM

O pad de áudio de `uridecodebin` fornece a trilha do arquivo já decodificada como `audio/x-raw`, ou seja, PCM. `entrada_audio` recebe essa trilha em uma `queue`. O programa abre o mesmo arquivo desde o início em A e B; portanto, as duas configurações processam o mesmo conteúdo de origem.

O formato, a taxa e os canais de entrada dependem do arquivo. No exemplo incluído, o áudio AAC tem 44.100 Hz e dois canais. AAC é apenas o codec armazenado no arquivo: os elementos seguintes trabalham com PCM após a decodificação.

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

A taxa de amostragem limita as frequências representáveis. Pelo critério de Nyquist, os limites teóricos são 24.000 Hz em A e 4.000 Hz em B. A reamostragem para B reduz componentes acima da faixa que 8.000 Hz consegue representar. A não recupera frequências ausentes na origem apenas por usar uma taxa maior. Use uma trilha com voz, música ou componentes agudos para comparar o som.

O arquivo `exemplo.mp4` contém uma onda dente de serra de 440 Hz gerada por `audiotestsrc`, com harmônicas que ajudam a perceber mudanças de timbre. O aplicativo lê esse arquivo; ele não cria um novo sinal de áudio durante A ou B.

Uma amostra de 16 bits dispõe de 65.536 níveis possíveis; uma de 8 bits, de 256. A representação de 8 bits tem passos de amplitude maiores e mais erro de quantização. Esses valores descrevem a capacidade do formato. O áudio escolhido não usa necessariamente todos os níveis possíveis.

A tem dois canais, B tem um. Se o arquivo tiver conteúdo estéreo distinto entre os canais, a conversão para mono perde essa separação espacial. No exemplo incluído, ambos os canais contêm o mesmo sinal, então a diferença espacial pode não ser perceptível. Converter uma origem mono para dois canais também não cria uma gravação estéreo original.

Para PCM sem compressão, a taxa de dados é `rate × bits por amostra × channels`:

- A: `48000 × 16 × 2 = 1536000 bit/s`, ou 192.000 bytes por segundo.
- B: `8000 × 8 × 1 = 64000 bit/s`, ou 8.000 bytes por segundo.

Para uma mesma duração, A usa aproximadamente 24 vezes mais dados PCM que B. O programa preserva a duração da trilha de áudio; podem existir pequenas diferenças de arredondamento na reamostragem. Os cerca de 5,02 segundos do exemplo incluem a duração de áudio resultante da codificação AAC.

## Tratamento de erros e encerramento

O programa verifica o argumento de entrada, a conversão do caminho em URI, erros de `gst_parse_launch()`, a criação da fonte e o retorno de `gst_element_set_state()`. O callback verifica falhas de ligação dos pads. Arquivos inexistentes, corrompidos ou sem os fluxos necessários produzem erros.

O Bus entrega mensagens dos elementos ao código da aplicação. `gst_bus_timed_pop_filtered()` aguarda `GST_MESSAGE_ERROR` ou `GST_MESSAGE_EOS`. Em um erro, o programa informa o elemento, a descrição e os detalhes disponíveis. Ao receber EOS, finaliza a configuração e libera os recursos antes de começar a seguinte.

O fim do arquivo encerra os fluxos. O EOS permite que `wavenc` finalize o cabeçalho do WAV. Depois, a aplicação passa a pipeline para `GST_STATE_NULL` e libera as referências, incluindo as filas obtidas com `gst_bin_get_by_name()`. Uma falha retorna código 1; o término normal das duas configurações retorna 0.

## Fontes e exemplos utilizados

Os trechos de inicialização, estado, Bus, tratamento de mensagens e entrada do macOS foram adaptados dos tutoriais oficiais. As descrições completas das nossas pipelines combinam esses exemplos com a documentação dos elementos.

| Referência | Uso nesta implementação |
|---|---|
| [Basic Tutorial 1](https://gstreamer.freedesktop.org/documentation/tutorials/basic/hello-world.html) | `gst_init`, `gst_parse_launch`, execução, limpeza e entrada no macOS |
| [Basic Tutorial 2](https://gstreamer.freedesktop.org/documentation/tutorials/basic/concepts.html) | Verificação de estado e `switch` para ERROR e EOS no Bus |
| [Basic Tutorial 3](https://gstreamer.freedesktop.org/documentation/tutorials/basic/dynamic-pipelines.html) | `CustomData`, criação de `uridecodebin`, propriedade `uri`, sinal `pad-added`, consulta de caps e ligação com `gst_pad_link` |
| [Basic Tutorial 6](https://gstreamer.freedesktop.org/documentation/tutorials/basic/media-formats-and-pad-capabilities.html) | Caps e negociação dos formatos de áudio e vídeo |
| [Basic Tutorial 10](https://gstreamer.freedesktop.org/documentation/tutorials/basic/gstreamer-tools.html) | Descrições textuais, caps filters, `tee name=...`, referências com ponto e `gst-inspect-1.0` |
| [Raw Audio Media Types](https://gstreamer.freedesktop.org/documentation/additional/design/mediatype-audio-raw.html) | Significado e formatos suportados de `format`, `rate`, `channels` e `layout` |
| [audioconvert](https://gstreamer.freedesktop.org/documentation/audioconvert/index.html) | Conversão de representação, profundidade e canais |
| [audioresample](https://gstreamer.freedesktop.org/documentation/audioresample/index.html) | Reamostragem do áudio bruto |
| [uridecodebin](https://gstreamer.freedesktop.org/documentation/playback/uridecodebin.html) | Abertura e decodificação do arquivo, com pads de saída dinâmicos |
| [URI helpers](https://gstreamer.freedesktop.org/documentation/gstreamer/gsturihandler.html#gst_filename_to_uri) | Conversão do caminho local em URI |
| [GstElement](https://gstreamer.freedesktop.org/documentation/gstreamer/gstelement.html#no-more-pads) | Sinal `no-more-pads` para identificar o fim da descoberta dos fluxos |
| [x264enc](https://gstreamer.freedesktop.org/documentation/x264/index.html) | Codificação H.264, formato I420 e parâmetros do encoder |
| [h264parse](https://gstreamer.freedesktop.org/documentation/videoparsersbad/h264parse.html) | Organização do fluxo H.264 |
| [avdec_h264](https://gstreamer.freedesktop.org/documentation/libav/avdec_h264.html) | Decodificação H.264 |
| [wavenc](https://gstreamer.freedesktop.org/documentation/wavenc/index.html) | WAV com amostras PCM, suporte a S16LE e U8 |
| [compositor](https://gstreamer.freedesktop.org/documentation/compositor/index.html) | Comparação visual e posição das entradas |

Escolhemos uma fonte de arquivo para experimentar o conteúdo trazido pelo grupo. `uridecodebin` permite usar formatos suportados pela instalação sem escrever um demuxer e um decoder para cada contêiner. Mantivemos a descrição textual dos ramos da atividade anterior e acrescentamos o callback do Tutorial 3 para completar as ligações dinâmicas.
