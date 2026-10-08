# Explicação do código linha a linha

O programa recebe um arquivo com vídeo e áudio. Em cada execução, mostra duas versões do mesmo vídeo lado a lado, reproduz a trilha sonora e grava essa trilha como PCM em WAV. Primeiro usa a configuração A, com 48.000 Hz, 16 bits e dois canais. Depois abre o mesmo arquivo desde o início e usa B, com 8.000 Hz, 8 bits e um canal.

O vídeo passa por uma etapa explícita de H.264. Após a decodificação dessa etapa, o programa divide os mesmos quadros entre a referência colorida e a versão menor, em cinza e com menos atualizações por segundo. A conexão da fonte aos ramos acontece durante a descoberta do arquivo, usando o callback do Tutorial 3.

Este guia explica o comportamento, as decisões de construção e as referências usadas. A numeração corresponde às 258 linhas de [main.c nesta revisão](https://github.com/frezatti/gstreamer-/blob/7d2dce5602c34fed5148dddeecb9ac7d4b1d6439/atividade_mpeg4_pcm/main.c). Os trechos abaixo vêm diretamente desse arquivo. Linhas em branco organizam a leitura; comentários e chaves são explicados com o bloco a que pertencem.

## De onde veio a implementação

A primeira atividade já usava uma descrição textual e uma comparação de vídeo com `tee` e `compositor`. A nova atividade acrescentou H.264 e PCM. O pedido para receber um vídeo trouxe a fonte `uridecodebin` e as ligações dinâmicas. Mantivemos os ramos da comparação e as duas configurações de áudio durante essa mudança.

| Referência | Padrão usado no código | Adaptação para a atividade |
|---|---|---|
| [Tutorial 1, Hello world](https://gstreamer.freedesktop.org/documentation/tutorials/basic/hello-world.html) | Inicialização, descrição com `gst_parse_launch`, espera por ERROR ou EOS e limpeza | Descrição própria dos ramos de vídeo e áudio |
| [Tutorial 2, GStreamer concepts](https://gstreamer.freedesktop.org/documentation/tutorials/basic/concepts.html) | Criação de elementos, propriedades, estados e tratamento de mensagens | Fonte adicionada à pipeline e mensagens de erro detalhadas |
| [Tutorial 3, Dynamic pipelines](https://gstreamer.freedesktop.org/documentation/tutorials/basic/dynamic-pipelines.html) | `CustomData`, `uridecodebin`, `pad-added`, consulta de caps e `gst_pad_link` | Seleção de vídeo e áudio. O exemplo original liga somente áudio |
| [Tutorial 6, Media formats and pad capabilities](https://gstreamer.freedesktop.org/documentation/tutorials/basic/media-formats-and-pad-capabilities.html) | Caps e negociação de formatos | Resolução, FPS, cor, formato PCM, taxa e canais definidos explicitamente |
| [Tutorial 10, GStreamer tools](https://gstreamer.freedesktop.org/documentation/tutorials/basic/gstreamer-tools.html) | Sintaxe textual, propriedades, referências a elementos nomeados e `gst-inspect-1.0` | Consulta dos elementos e construção dos ramos com nomes |
| Documentação dos elementos | Formatos aceitos e função de cada transformação | Escolha de `x264enc`, `h264parse`, `avdec_h264`, conversores, `compositor` e `wavenc` |

Essa combinação não aparece pronta em um dos tutoriais. Os tutoriais fornecem os padrões de uso da biblioteca. A sequência dos elementos, as propriedades e a organização das saídas correspondem à nossa aplicação. Os valores 640 × 480, 320 × 240, 30 FPS e 10 FPS mantêm a comparação da atividade anterior. Os dois formatos PCM tornam observáveis as mudanças de taxa, profundidade e canais.

## A ordem em que o programa realmente executa

O arquivo apresenta `tutorial_main()` antes de `main()`, mas a execução de um programa C começa em `main()`. A posição de uma função no arquivo não determina quando ela será chamada.

1. `main()` encaminha a execução para `tutorial_main()`, usando a entrada específica do macOS quando necessário.
2. `tutorial_main()` inicializa GStreamer, verifica o argumento e começa a configuração A.
3. O parser cria os ramos. O programa acrescenta a fonte, define sua URI, registra os callbacks e solicita `PLAYING`.
4. GStreamer descobre os fluxos, chama `pad_added_handler()` e passa a processar a mídia. Enquanto isso, a aplicação espera mensagens no Bus.
5. No fim do arquivo, a aplicação recebe EOS, encerra e libera a pipeline. O `for` repete a construção para B. Após B, o programa retorna 0.

Os callbacks não são chamados porque a execução chegou à linha onde eles foram escritos. Eles são funções que GStreamer chama em resposta aos sinais registrados. Os diagramas completos estão em [vídeo](diagramas/video.excalidraw), [áudio A](diagramas/audio_a.excalidraw) e [áudio B](diagramas/audio_b.excalidraw).

## Linhas 1 a 13. Origem e cabeçalhos

```c
/*
 * Adaptacao dos tutoriais basicos 1, 2, 3, 6 e 10 do GStreamer.
 * Mantem a comparacao de video da atividade anterior.
 * Le um arquivo com video e audio, usando pads dinamicos.
 * Mantem H.264 e duas configuracoes de audio PCM.
 * Os exemplos e a documentacao dos elementos estao no README.md.
 */

#include <gst/gst.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif
```

As linhas 1 a 7 formam um comentário de várias linhas. `/*` inicia o comentário e `*/` o encerra. A linha 2 identifica os tutoriais usados. As linhas 3 a 5 descrevem as funções mantidas na aplicação. A linha 6 aponta para a documentação do projeto. O compilador ignora esse texto.

Na linha 9, `#include <gst/gst.h>` fornece as declarações da biblioteca. É por esse cabeçalho que o compilador reconhece nomes como `GstElement`, `GstBus` e `gst_init`. O cabeçalho também inclui as dependências GLib necessárias aos tipos e funções `g_` usados aqui. Incluir o cabeçalho informa as declarações; a ligação com as bibliotecas ocorre na compilação, conforme o README.

A linha 11 testa uma macro definida pelo ambiente de compilação da Apple. A linha 12 inclui `TargetConditionals.h` somente nessa plataforma. Esse cabeçalho fornece as macros usadas no `main()` para identificar o alvo. A linha 13 fecha a condição do pré-processador. `#ifdef` escolhe o código que será compilado, antes da execução. No Windows, esse include não entra.

Essa organização dos includes e da entrada da Apple foi preservada dos tutoriais, para manter o código próximo dos exemplos recomendados.

## Linhas 15 a 25. Dados compartilhados e declarações dos callbacks

```c
/* Como no tutorial 3: compartilha os elementos com os callbacks. */
typedef struct _CustomData {
    GstElement *pipeline;
    GstElement *source;
    GstElement *video_queue;
    GstElement *audio_queue;
} CustomData;

static void pad_added_handler(GstElement *src, GstPad *new_pad,
                              CustomData *data);
static void no_more_pads_handler(GstElement *src, CustomData *data);
```

A linha 15 explica por que existe `CustomData`. Precisamos entregar aos callbacks os elementos aos quais eles devem ligar os novos pads.

Na linha 16, `struct _CustomData` inicia uma estrutura C. Uma estrutura reúne campos relacionados em um único valor. `typedef` cria um nome curto para esse tipo. A linha 21 fecha a definição e estabelece o nome `CustomData` que usamos nas declarações seguintes.

Cada uma das linhas 17 a 20 guarda um ponteiro para um elemento. `pipeline` é o conjunto que controla a execução. `source` é o `uridecodebin`. `video_queue` aponta para a fila de entrada do vídeo. `audio_queue` aponta para a fila de entrada do áudio. O `*` indica que guardamos o endereço do objeto, e não uma cópia do elemento.

As linhas 23 e 24 declaram `pad_added_handler()` antes de sua implementação. Seus argumentos serão a fonte que emitiu o sinal, o pad recém-criado e o endereço de `CustomData`. A linha 25 declara o segundo callback, que recebe a fonte e os mesmos dados, mas não recebe um pad individual.

`static` limita essas funções ao arquivo C. `void` informa que elas não devolvem um valor. O ponto e vírgula encerra a declaração. Os corpos aparecem mais abaixo, a partir das linhas 189 e 235.

O Tutorial 3 usa essa mesma organização com `CustomData` e um callback. Adaptamos os campos para guardar as duas filas de entrada, porque a nossa aplicação processa ambos os tipos de mídia.

## Linhas 27 a 30. Função principal da atividade e descrição do vídeo

```c
int tutorial_main(int argc, char *argv[])
{
    /* Este fluxo de video sera usado nas duas configuracoes de audio. */
    const gchar *video =
```

A linha 27 define `tutorial_main()`. Seu retorno `int` indica sucesso ou falha ao código que a chamou. `argc` informa quantos argumentos existem e `argv` contém os textos desses argumentos. A linha 28 abre o corpo da função.

A linha 29 registra que o ramo de vídeo será usado em A e B. A diferença entre as duas execuções está nas caps do áudio.

Na linha 30, `const gchar *video` guarda o endereço de uma string. `gchar` é o tipo de caractere da GLib. `const` indica que não vamos modificar os caracteres dessa descrição. A atribuição continua nas linhas seguintes e termina na linha 46.

Aqui ainda não criamos elementos GStreamer. Estamos apenas preparando o texto que descreve esses elementos. Em C, strings literais colocadas uma após a outra são concatenadas pelo compilador. As várias linhas entre aspas formam uma única string, na mesma ordem em que foram escritas.

## Linhas 31 a 33. Adaptação do vídeo de entrada

```c
        "queue name=entrada_video ! videoconvert ! videoscale ! videorate ! "
        "video/x-raw,format=I420,width=640,height=480,framerate=30/1,"
        "pixel-aspect-ratio=1/1 ! "
```

A linha 31 começa com `queue name=entrada_video`. Essa fila receberá o pad de vídeo criado por `uridecodebin`. O nome permite encontrá-la depois, na linha 121. Uma `queue` armazena temporariamente buffers e cria uma thread para a parte seguinte do fluxo. Essa separação ajuda os ramos de áudio e vídeo a continuar seu processamento.

Ainda na linha 31, `videoconvert` adapta o formato dos pixels, `videoscale` adapta a resolução e `videorate` adapta a sequência de quadros ao FPS solicitado. O símbolo `!`, dentro do texto, instrui o parser a conectar elementos. Ele pertence à sintaxe da descrição GStreamer.

A linha 32 fixa as caps desejadas depois desses elementos. `video/x-raw` significa vídeo bruto, já decodificado. `format=I420` seleciona vídeo YUV planar com subamostragem de cor 4:2:0. `width=640` e `height=480` definem as dimensões. `framerate=30/1` significa 30 quadros por segundo, expresso como uma fração.

A linha 33 completa essas caps com `pixel-aspect-ratio=1/1`, que pede pixels quadrados, e conecta o resultado ao encoder da linha seguinte. O espaço no fim da string evita que os tokens se juntem ao texto seguinte.

Essas caps restringem o formato negociado. Quem transforma os dados são os conversores anteriores. Um arquivo de 320 × 240 a 24 FPS, como o exemplo incluído, é adaptado para 640 × 480 a 30 FPS. `videorate` pode repetir ou remover quadros mantendo os tempos do vídeo; aumentar o FPS não cria detalhes de movimento que não existiam na origem.

Mantivemos uma referência com dimensões conhecidas porque o compositor usa posições fixas na comparação. O formato I420 também é aceito pelo encoder escolhido. Os conceitos de caps vêm do Tutorial 6; o comportamento dos elementos está em [videoconvert](https://gstreamer.freedesktop.org/documentation/videoconvertscale/videoconvert.html), [videoscale](https://gstreamer.freedesktop.org/documentation/videoconvertscale/videoscale.html) e [videorate](https://gstreamer.freedesktop.org/documentation/videorate/index.html).

## Linhas 34 a 36. Etapa explícita de H.264

```c
        "x264enc name=codificador tune=zerolatency "
        "speed-preset=ultrafast bitrate=1000 ! "
        "h264parse name=h264 ! avdec_h264 name=decodificador ! "
```

A linha 34 cria `x264enc`, o encoder que transforma os quadros brutos em H.264. `name=codificador` dá um nome à instância. `tune=zerolatency` reduz o acúmulo de quadros no encoder, o que ajuda a limitar a espera em uma pipeline que também reproduz áudio.

A linha 35 completa as propriedades do encoder. `speed-preset=ultrafast` prioriza velocidade de codificação. `bitrate=1000` estabelece uma taxa alvo de 1.000 kbit/s. É um valor escolhido para esta demonstração, não uma afirmação de qualidade ideal para todos os vídeos. A taxa alvo também não significa que cada segundo terá exatamente esse tamanho.

Na linha 36, `h264parse name=h264` interpreta e organiza o fluxo comprimido. Em seguida, `avdec_h264 name=decodificador` reconstrói quadros brutos a partir dele. Entre encoder, parser e decoder, a mídia usa `video/x-h264`.

Essa etapa atende diretamente ao requisito de H.264/MPEG-4 AVC. Um arquivo `.mp4` é um contêiner e sua extensão, sozinha, não comprova que o vídeo usa H.264. A fonte aceita os formatos para os quais há plugins instalados, mas este ramo utiliza H.264 explicitamente.

Se a entrada já for H.264, `uridecodebin` a decodifica para fornecer os quadros à aplicação. Esses quadros passam novamente pelo encoder e pelo decoder mostrados aqui. Mantivemos esse ciclo da atividade para demonstrar onde o fluxo comprimido existe. Ele acrescenta processamento e pode introduzir perdas de compressão.

A escolha desses elementos e propriedades veio de [x264enc](https://gstreamer.freedesktop.org/documentation/x264/index.html), [h264parse](https://gstreamer.freedesktop.org/documentation/videoparsersbad/h264parse.html) e [avdec_h264](https://gstreamer.freedesktop.org/documentation/libav/avdec_h264.html).

## Linhas 37 a 42. Dois ramos para os mesmos quadros

```c
        "videoconvert ! video/x-raw,format=RGB ! "
        "tee name=v "
        "v. ! queue ! mix.sink_0 "
        "v. ! queue ! videoscale ! videorate ! videoconvert ! "
        "video/x-raw,format=GRAY8,width=320,height=240,framerate=10/1 ! "
        "mix.sink_1 "
```

A linha 37 converte o resultado decodificado para RGB. As caps `video/x-raw,format=RGB` pedem essa representação dos pixels. Isso prepara a referência colorida e a conversão posterior para cinza.

Na linha 38, `tee name=v` divide um fluxo entre várias saídas. Os dois ramos recebem os mesmos quadros de origem. É por esse elemento que comparamos duas versões do vídeo sem abrir uma segunda fonte para a imagem da direita.

A linha 39 descreve o ramo da referência. `v.` referencia o `tee` chamado `v`. O parser cria a ligação a uma saída desse elemento. O fluxo passa por `queue` e chega ao pad `sink_0` do compositor chamado `mix`. O ponto em `mix.sink_0` indica um pad específico do elemento nomeado.

A linha 40 inicia o segundo ramo usando outra saída de `v`. A fila separa seu processamento. `videoscale`, `videorate` e `videoconvert` permitem alterar tamanho, FPS e formato de cor.

A linha 41 pede `GRAY8`, 320 × 240 e `10/1`. GRAY8 representa cada pixel por um valor de cinza de 8 bits. O ramo produz 10 quadros por segundo com essas dimensões. A linha 42 entrega esse resultado ao pad `sink_1` do mesmo compositor.

Os nomes podem aparecer antes da definição textual do elemento a que se referem. O parser resolve essas referências ao construir a descrição completa. Os pads de saída solicitados ao `tee` e os pads do compositor são preparados pelo parser. O callback que escrevemos depois cuida dos pads de saída da fonte `uridecodebin`.

As filas são necessárias para permitir que os ramos divididos funcionem em threads separadas. O uso de nomes e referências segue a sintaxe do Tutorial 10. A função de dividir está documentada em [tee](https://gstreamer.freedesktop.org/documentation/coreelements/tee.html) e o processamento das filas em [queue](https://gstreamer.freedesktop.org/documentation/coreelements/queue.html).

## Linhas 43 a 46. Composição e saída do vídeo

```c
        "compositor name=mix background=black "
        "sink_0::xpos=0 sink_1::xpos=640 sink_1::ypos=120 ! "
        "video/x-raw,width=960,height=480,framerate=30/1 ! "
        "videoconvert ! autovideosink ";
```

A linha 43 cria `compositor name=mix` e seleciona fundo preto. Esse elemento recebe os dois ramos e produz uma única imagem de saída.

A linha 44 configura propriedades dos pads do compositor. `sink_0::xpos=0` posiciona a referência à esquerda. `sink_1::xpos=640` inicia a versão processada imediatamente depois da largura de 640 pixels da referência. `sink_1::ypos=120` centraliza verticalmente a imagem de 240 pixels dentro da altura de 480. O deslocamento é `(480 - 240) / 2 = 120`. O separador `::` permite configurar uma propriedade do pad filho nessa descrição textual.

A linha 45 define o resultado composto como vídeo de 960 × 480 a 30 FPS. A largura soma 640 pixels da referência e 320 da versão processada. A altura comporta a referência inteira e as margens pretas da versão menor.

O compositor gera quadros de saída a 30 FPS, mas o ramo direito tem apenas 10 atualizações por segundo. A composição pode reutilizar a mesma imagem da direita em vários quadros consecutivos. Isso não transforma o ramo direito em 30 imagens diferentes por segundo.

A linha 46 usa outro `videoconvert` para adaptar a composição ao formato aceito pela saída e a entrega a `autovideosink`, que seleciona um sink de vídeo disponível. As aspas terminam e o ponto e vírgula encerra a atribuição iniciada na linha 30.

Essa comparação foi mantida da primeira atividade. Ambos os lados já passaram pelo ciclo H.264. Portanto, a diferença observada demonstra tamanho, cor e frequência de atualização após a decodificação. As posições e a composição seguem a documentação de [compositor](https://gstreamer.freedesktop.org/documentation/compositor/index.html).

## Linhas 48 a 53. Duas representações PCM

```c
    /* Apenas estas caps mudam entre A e B. */
    const gchar *configuracoes[] = {
        "audio/x-raw,format=S16LE,rate=48000,channels=2,layout=interleaved",
        "audio/x-raw,format=U8,rate=8000,channels=1,layout=interleaved"
    };
    const gchar *arquivos[] = { "pcm_a.wav", "pcm_b.wav" };
```

A linha 48 explica que as caps escolhidas para áudio mudam entre A e B. A linha 49 declara um array de ponteiros para strings chamado `configuracoes`.

A linha 50 é a entrada de índice 0, a configuração A. `audio/x-raw` identifica áudio bruto, ou PCM. `S16LE` significa amostras inteiras com sinal, com 16 bits e ordem little-endian. `rate=48000` significa 48.000 amostras por segundo em cada canal. `channels=2` pede dois canais. `layout=interleaved` pede amostras dos canais intercaladas no buffer.

A linha 51 é a entrada de índice 1, a configuração B. `U8` significa inteiro sem sinal de 8 bits. A taxa passa a 8.000 Hz e o áudio passa a um canal. Para uma representação intercalada com dois canais, a sequência contém uma amostra do primeiro canal, uma do segundo, depois o próximo par. Com um canal, existe somente uma sequência de amostras.

A linha 52 encerra o array. A linha 53 declara outro array, `arquivos`, com os nomes de saída na mesma ordem. O índice 0 combina A com `pcm_a.wav`. O índice 1 combina B com `pcm_b.wav`.

Mudamos três características: taxa de amostragem, profundidade/representação e quantidade de canais. Os valores são configurações da experiência, não requisitos fixos impostos pelo enunciado. A documentação de [áudio bruto](https://gstreamer.freedesktop.org/documentation/additional/design/mediatype-audio-raw.html) define esses campos. [wavenc](https://gstreamer.freedesktop.org/documentation/wavenc/index.html) aceita os dois formatos PCM selecionados.

## Linhas 55 a 61. Inicialização e argumento de entrada

```c
    gst_init(&argc, &argv);

    if (argc != 2) {
        g_printerr("Uso: %s \"video.mp4\"\n", argv[0]);
        g_printerr("O arquivo deve conter video e audio.\n");
        return 1;
    }
```

A linha 55 chama `gst_init()` antes de qualquer operação da biblioteca. GStreamer inicializa suas estruturas e processa as opções de linha de comando que reconhece. Os endereços `&argc` e `&argv` permitem que a função ajuste os argumentos, retirando as opções consumidas pela biblioteca.

A linha 57 verifica os argumentos restantes. Para `atividade.exe exemplo.mp4`, `argc` vale 2: o nome do programa e o caminho do arquivo. `argv[0]` contém o nome do programa e `argv[1]` contém `exemplo.mp4`. Um caminho entre aspas, mesmo contendo espaços, deve chegar como um único argumento.

A linha 58 imprime o formato de uso. `%s` será substituído por `argv[0]`. A sequência `\"` no código representa aspas dentro da string C, e `\n` representa quebra de linha. A linha 59 informa a exigência de duas mídias. Essas mensagens usam `g_printerr`, a saída de erro.

A linha 60 encerra a função com código 1 quando a quantidade está incorreta. Esse valor indica falha ao chamador e, ao final, ao sistema operacional. A linha 61 fecha o `if`. Com dois argumentos, o programa continua.

A inicialização segue o Tutorial 1. A verificação do caminho é uma adaptação para a entrada escolhida pelo usuário. Atualmente, as caps ficam no código e o argumento identifica o arquivo a processar.

## Linhas 63 a 71. Repetição para A e B e variáveis da execução

```c
    /* Executa o arquivo inteiro em A e depois abre o mesmo arquivo em B. */
    for (guint i = 0; i < G_N_ELEMENTS(configuracoes); i++) {
        CustomData data = { 0 };
        GstBus *bus;
        GstMessage *msg;
        GstStateChangeReturn ret;
        GError *error = NULL;
        int resultado = 0;
        gchar *uri = gst_filename_to_uri(argv[1], &error);
```

A linha 63 descreve a repetição do mesmo arquivo. A linha 64 inicia o `for` com três partes. `guint i = 0` começa no primeiro índice. `i < G_N_ELEMENTS(configuracoes)` mantém o laço enquanto houver uma configuração. `i++` incrementa o índice após cada execução. `guint` é um inteiro sem sinal da GLib e `G_N_ELEMENTS` calcula a quantidade de entradas do array, que aqui é 2.

Esse laço percorre configurações, não elementos separados por `!`. Cada passagem constrói uma pipeline inteira. Na primeira, usa `configuracoes[0]` e `arquivos[0]`. Na segunda, usa o índice 1. O Bus mantém a execução esperando o fim da configuração atual antes de começar a seguinte.

A linha 65 cria `CustomData data` e zera seus campos. Para esses campos de ponteiro, isso estabelece inicialmente `NULL`. `data` existe durante a passagem atual do `for`. Entregamos seu endereço aos callbacks e paramos a pipeline antes de sair desse escopo.

A linha 66 declara o ponteiro para o Bus. A linha 67 declara a mensagem que ele devolverá. A linha 68 declara `ret`, que recebe o resultado de uma mudança de estado. A linha 69 começa `error` com `NULL`, indicando ausência de erro. A linha 70 começa `resultado` em 0, indicando sucesso, até que alguma falha seja identificada.

A linha 71 transforma `argv[1]` em uma URI aceita pela fonte. `gst_filename_to_uri()` resolve caminhos relativos com base na pasta atual e trata a representação de um caminho em uma URI de arquivo. `&error` permite que a função registre uma falha. Ela devolve uma string nova, que a aplicação precisa liberar depois.

Essa conversão mantém o caminho fora da descrição textual da pipeline. O arquivo pode ter espaços sem ser interpretado como tokens de elementos. A referência específica é [gst_filename_to_uri](https://gstreamer.freedesktop.org/documentation/gstreamer/gsturihandler.html#gst_filename_to_uri).

## Linhas 73 a 82. Erro na URI e preparação dos ramos

```c
        if (uri == NULL) {
            g_printerr("Caminho invalido: %s\n", error->message);
            g_clear_error(&error);
            return 1;
        }

        /*
         * Prepara os dois ramos, ainda sem ligar a fonte.
         * O callback pad_added_handler fara essa ligacao, como no tutorial 3.
         */
```

A linha 73 verifica se a conversão devolveu `NULL`. Nesse caso, a URI não foi criada. A linha 74 imprime a mensagem disponível em `error->message`. O operador `->` acessa um campo de uma estrutura apontada por um ponteiro.

A linha 75 libera o `GError` e coloca o ponteiro em `NULL`, usando `g_clear_error(&error)`. A linha 76 retorna 1 e a linha 77 fecha esse tratamento. Nenhuma pipeline foi criada ainda, então não há objetos da pipeline para liberar nesse ponto.

As linhas 79 a 82 são um comentário sobre a etapa seguinte. Vamos construir os ramos de processamento, mas ainda não conectá-los à fonte. Os pads de saída dessa fonte dependem do conteúdo descoberto no arquivo. Essa separação aplica o padrão do Tutorial 3: preparar o destino e completar a ligação quando o pad aparecer.

## Linhas 83 a 90. Construção da descrição completa e do áudio

```c
        gchar *descricao = g_strdup_printf(
            "%s "
            "queue name=entrada_audio ! "
            "audioconvert ! audioresample ! %s ! "
            "tee name=a "
            "a. ! queue ! audioconvert ! audioresample ! autoaudiosink "
            "a. ! queue ! wavenc ! filesink location=%s",
            video, configuracoes[i], arquivos[i]);
```

A linha 83 chama `g_strdup_printf()`. Ela funciona como uma formatação de texto, mas devolve uma string nova, alocada em memória. O ponteiro `descricao` guarda essa string completa.

A linha 84 insere o primeiro `%s`, substituído pelo ramo de vídeo declarado antes. A linha 85 começa o áudio com `queue name=entrada_audio`. Essa fila receberá o pad de áudio decodificado.

A linha 86 coloca `audioconvert` e `audioresample` antes do segundo `%s`. `audioconvert` permite mudar representação, profundidade e canais. `audioresample` permite mudar a taxa de amostragem. O segundo `%s` receberá as caps de A ou B, que esses elementos deverão atender.

A linha 87 cria `tee name=a` para dividir o PCM já convertido. A linha 88 define sua saída de reprodução: fila, conversão e reamostragem de adaptação ao dispositivo, e `autoaudiosink`. O hardware pode exigir uma representação diferente, então esses últimos conversores ajudam a saída a negociar seu formato.

A linha 89 define a saída de arquivo usando outra fila, `wavenc` e `filesink`. `wavenc` organiza as amostras PCM em um contêiner WAV. `filesink location=%s` grava o nome fornecido pelo terceiro `%s`. Os dados desse ramo conservam as caps selecionadas, porque aqui não acrescentamos conversores depois da divisão.

A linha 90 entrega os três valores a `g_strdup_printf`, na ordem dos três marcadores: `video`, `configuracoes[i]` e `arquivos[i]`. A expressão termina com `);`.

Vídeo e áudio entram em uma única descrição e pertencem à mesma pipeline. A fonte será comum aos dois. O uso das caps aplica o Tutorial 6 e as funções de transformação seguem [audioconvert](https://gstreamer.freedesktop.org/documentation/audioconvert/index.html) e [audioresample](https://gstreamer.freedesktop.org/documentation/audioresample/index.html).

## Linhas 92 a 95. Mensagens para acompanhar a demonstração

```c
        g_print("\nConfiguracao %c: %s\n", 'A' + (int) i, configuracoes[i]);
        g_print("Arquivo de entrada: %s.\n", argv[1]);
        g_print("Arquivo de saida: %s.\n", arquivos[i]);
        g_print("Video: H.264, com comparacao lado a lado.\n");
```

A linha 92 imprime a configuração atual. `'A' + (int) i` produz o caractere A quando `i` vale 0 e B quando vale 1. `%c` imprime esse caractere. `%s` imprime a string das caps. O cast `(int)` converte o índice para o tipo usado nessa expressão.

A linha 93 mostra o arquivo de entrada. A linha 94 mostra o nome de saída associado à configuração. A linha 95 informa o processamento de vídeo e a comparação lado a lado.

`g_print()` escreve na saída normal do terminal. Essas chamadas ajudam a identificar qual execução está ativa. Mostrar o nome de saída não significa que o arquivo já terminou de ser gravado; a confirmação de conclusão depende do EOS.

## Linhas 97 a 108. Parser e limpeza após uma falha de construção

```c
        /* Constroi a pipeline pelo texto, como no tutorial 1. */
        data.pipeline = gst_parse_launch(descricao, &error);
        g_free(descricao);
        if (error != NULL || data.pipeline == NULL) {
            g_printerr("Erro ao criar a pipeline: %s\n",
                       error != NULL ? error->message : "erro desconhecido");
            g_clear_error(&error);
            g_free(uri);
            if (data.pipeline != NULL)
                gst_object_unref(data.pipeline);
            return 1;
        }
```

A linha 97 identifica a referência do Tutorial 1. Na linha 98, `gst_parse_launch()` interpreta `descricao`, cria os elementos, define propriedades e resolve as ligações entre os ramos. O resultado vai para `data.pipeline`. `&error` permite receber a descrição de uma falha.

A linha 99 libera o texto temporário. Depois que o parser terminou, a string já cumpriu sua função. Liberar `descricao` não destrói os elementos criados.

A linha 100 verifica duas condições com `||`, o operador lógico OU: houve erro, ou a pipeline não foi criada. A documentação do parser permite que ele devolva um objeto parcialmente construído junto com um erro. Por isso, verificar somente se o ponteiro é `NULL` seria insuficiente para a política de interromper em qualquer falha de construção adotada aqui.

As linhas 101 e 102 imprimem uma mensagem. `condicao ? valor_se_verdadeiro : valor_se_falso` é o operador ternário de C. Ele seleciona `error->message` quando o erro existe, ou o texto de fallback quando não existe.

A linha 103 libera o erro. A linha 104 libera a URI que ainda não foi entregue à fonte. A linha 105 verifica se existe um objeto parcial; a linha 106 libera a referência desse objeto quando existir. Isso tem utilidade mesmo com o `return 1` da linha 107: retornar de uma função C não libera automaticamente os objetos alocados pela biblioteca. A linha 108 fecha o `if`.

O tratamento evita continuar com uma pipeline incompleta e cobre, por exemplo, plugins ausentes. O detalhe sobre resultado parcial está em [gst_parse_launch](https://gstreamer.freedesktop.org/documentation/gstreamer/gstparse.html#gst_parse_launch).

## Linhas 110 a 120. Criação da fonte e adição à pipeline

```c
        /* Cria e configura a fonte pelo mesmo padrao do tutorial 3. */
        data.source = gst_element_factory_make("uridecodebin", "source");
        if (data.source == NULL ||
            !gst_bin_add(GST_BIN(data.pipeline), data.source)) {
            g_printerr("Nao foi possivel criar ou adicionar uridecodebin.\n");
            if (data.source != NULL)
                gst_object_unref(data.source);
            g_free(uri);
            gst_object_unref(data.pipeline);
            return 1;
        }
```

A linha 110 aponta a inspiração no Tutorial 3. A linha 111 cria uma instância de `uridecodebin` chamada `source`. O primeiro texto é o nome da fábrica do plugin; o segundo é o nome dado à instância.

`uridecodebin` recebe uma URI e escolhe internamente os elementos necessários para ler, separar e decodificar o arquivo. Seus pads de saída fornecem mídia bruta. A aplicação configura os ramos que receberão essa mídia.

As linhas 112 e 113 verificam criação e adição. O operador `||` faz avaliação de curto-circuito: se `data.source == NULL` for verdadeiro, a chamada a `gst_bin_add` não acontece. Isso evita tentar adicionar uma fonte inexistente.

`GST_BIN(data.pipeline)` permite tratar a pipeline como um bin, que é um contêiner de elementos. `gst_bin_add()` coloca a fonte nesse contêiner. Adicionar o elemento não o conecta às filas. Essas conexões ainda dependem dos pads dinâmicos.

A linha 114 mostra a falha. As linhas 115 e 116 liberam a fonte se ela foi criada, mas não pôde ser adicionada. A linha 117 libera a URI, a linha 118 libera a pipeline e a linha 119 retorna 1. A linha 120 fecha a condição.

Após uma adição bem-sucedida, a pipeline passa a possuir a fonte. Esse vínculo explica por que a limpeza normal não faz um `unref` separado de `data.source`: ela será liberada com seu contêiner. A escolha da fonte segue [uridecodebin](https://gstreamer.freedesktop.org/documentation/playback/uridecodebin.html).

## Linhas 121 a 127. Elementos nomeados, URI e sinais

```c
        data.video_queue = gst_bin_get_by_name(GST_BIN(data.pipeline), "entrada_video");
        data.audio_queue = gst_bin_get_by_name(GST_BIN(data.pipeline), "entrada_audio");
        g_object_set(data.source, "uri", uri, NULL);
        g_free(uri);

        g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler), &data);
        g_signal_connect(data.source, "no-more-pads", G_CALLBACK(no_more_pads_handler), &data);
```

A linha 121 busca `entrada_video` dentro da pipeline pelo nome configurado na descrição. A linha 122 faz o mesmo com `entrada_audio`. Essas buscas não criam filas novas. Elas devolvem referências às filas já criadas pelo parser, que serão entregues aos callbacks.

A linha 123 define a propriedade `uri` da fonte com `g_object_set()`. Os argumentos são o objeto, o nome da propriedade, o valor e um `NULL` que encerra a lista de propriedades. A fonte guarda a string da propriedade. Por isso, a linha 124 pode liberar a string temporária `uri` sem remover o valor configurado na fonte.

A linha 126 registra o callback para `pad-added`. O nome do sinal identifica o evento. `G_CALLBACK(pad_added_handler)` fornece o ponteiro da função no tipo de callback usado pela GLib. `&data` entrega o endereço da estrutura que o callback receberá como último argumento.

A linha 127 registra `no_more_pads_handler` para `no-more-pads`, usando os mesmos dados. Registrar um callback prepara uma chamada futura; essas linhas não executam os corpos dos handlers.

Quando a fonte criar um pad, ela chamará o primeiro handler. Quando informar que terminou de expor os pads, chamará o segundo. A vida de `data` precisa abranger essas chamadas. Neste código, a estrutura permanece no escopo da execução até a pipeline ser colocada em `NULL`.

O padrão de `g_signal_connect`, `G_CALLBACK` e dados compartilhados é o do Tutorial 3. A verificação com `no-more-pads` é a adaptação para impedir espera por um fluxo que não existe no arquivo. A busca por nome acrescenta referências próprias, que liberamos nas linhas 177 e 178.

## Linhas 129 a 138. Solicitação do estado PLAYING

```c
        /* Inicia a reproducao, seguindo o tutorial 2. */
        ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            g_printerr("Nao foi possivel iniciar a reproducao.\n");
            gst_element_set_state(data.pipeline, GST_STATE_NULL);
            gst_object_unref(data.video_queue);
            gst_object_unref(data.audio_queue);
            gst_object_unref(data.pipeline);
            return 1;
        }
```

A linha 129 registra a referência do Tutorial 2. A linha 130 solicita `GST_STATE_PLAYING` para a pipeline inteira. GStreamer conduz as transições necessárias dos elementos e inicia o processamento da mídia.

As fases são `NULL`, sem execução; `READY`, com preparação de recursos; `PAUSED`, com preparação inicial da mídia; e `PLAYING`, com o fluxo seguindo o relógio da pipeline. Uma solicitação para `PLAYING` pode continuar de forma assíncrona. O retorno da função indica o resultado da solicitação, não a conclusão de todo o vídeo.

A linha 131 interrompe somente se o resultado for `GST_STATE_CHANGE_FAILURE`. A linha 132 imprime a mensagem. A linha 133 devolve a pipeline a `NULL`, para parar e liberar recursos de execução que tenham sido iniciados.

As linhas 134 e 135 liberam as referências às filas obtidas por nome. A linha 136 libera a referência à pipeline. A linha 137 retorna 1 e a linha 138 fecha o tratamento da falha.

É depois de começar a mudança de estado que a fonte pode ler o arquivo e descobrir seus fluxos. Nesse processo, os callbacks fazem as ligações que ainda faltam. O mecanismo dos estados está descrito nos Tutoriais 2 e 3 e em [gst_element_set_state](https://gstreamer.freedesktop.org/documentation/gstreamer/gstelement.html#gst_element_set_state).

## Linhas 140 a 147. Bus e espera por encerramento

```c
        /* Aguarda um erro ou o fim de todos os fluxos da pipeline. */
        bus = gst_element_get_bus(data.pipeline);
        msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                        GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

        if (msg != NULL) {
            GError *err;
            gchar *debug_info;
```

A linha 140 descreve o que aguardamos. A linha 141 obtém o Bus da pipeline. O Bus transporta mensagens dos elementos e das threads de processamento até a aplicação. Ele informa situações como falhas, mudanças de estado e fim de reprodução. Os buffers com pixels e amostras percorrem os pads da pipeline; as mensagens de controle chegam por esse Bus.

Na linha 142, `gst_bus_timed_pop_filtered()` espera e retira uma mensagem cujo tipo corresponda ao filtro. Ela descarta as mensagens de outros tipos encontradas durante essa espera. `GST_CLOCK_TIME_NONE` escolhe uma espera sem limite de tempo. A linha 143 combina ERROR e EOS com `|`, o operador de OU bit a bit, formando a máscara de tipos aceita.

Uma mensagem de erro pode vir de um arquivo que não abriu, um decoder, uma negociação incompatível ou do nosso próprio handler. A mensagem de EOS chega à aplicação quando os sinks da pipeline concluíram seus fluxos. A função de espera não cria essas mensagens; ela as recebe.

A espera bloqueia a thread da aplicação nesse ponto. As threads do GStreamer continuam processando áudio e vídeo e podem chamar os handlers. Assim, o programa não precisa percorrer os quadros ou amostras em um `for` C.

A linha 145 verifica se houve uma mensagem. As linhas 146 e 147 declaram os ponteiros que serão preenchidos ao analisar um erro. O Bus e a mensagem recebida têm referências que a aplicação libera posteriormente. O padrão de espera vem do Tutorial 1; os detalhes estão em [GstBus](https://gstreamer.freedesktop.org/documentation/gstreamer/gstbus.html#gst_bus_timed_pop_filtered).

## Linhas 149 a 159. Leitura de uma mensagem de erro

```c
            switch (GST_MESSAGE_TYPE(msg)) {
                case GST_MESSAGE_ERROR:
                    gst_message_parse_error(msg, &err, &debug_info);
                    g_printerr("Erro no elemento %s: %s\n",
                               GST_OBJECT_NAME(msg->src), err->message);
                    g_printerr("Detalhes: %s\n",
                               debug_info ? debug_info : "nenhum");
                    g_clear_error(&err);
                    g_free(debug_info);
                    resultado = 1;
                    break;
```

A linha 149 inicia um `switch` sobre o tipo da mensagem, extraído por `GST_MESSAGE_TYPE(msg)`. A linha 150 seleciona o caso de erro. Esse tratamento segue o exemplo do Tutorial 2.

A linha 151 chama `gst_message_parse_error()`. A função extrai um `GError` para `err` e uma string de detalhes para `debug_info`. Os argumentos usam `&` porque a função precisa preencher esses ponteiros.

As linhas 152 e 153 imprimem o elemento que originou a mensagem e o texto do erro. `msg->src` identifica a origem; `GST_OBJECT_NAME()` fornece seu nome. Essa indicação ajuda a localizar uma falha no encoder, na fonte ou na saída.

As linhas 154 e 155 imprimem os detalhes, se existirem. O operador ternário seleciona `debug_info` ou o texto `nenhum` quando esse ponteiro é nulo.

A linha 156 libera `err`, a linha 157 libera a string de detalhes e a linha 158 registra a falha em `resultado`. A linha 159 usa `break` para sair do `switch`. Ela não encerra o `for` nesse ponto; a limpeza comum da execução ainda será feita antes do retorno de falha.

Essa escolha concentra o encerramento normal da execução depois da leitura da mensagem. O erro não é ignorado: ele impede começar B quando A falha. A função de leitura da mensagem está em [GstMessage](https://gstreamer.freedesktop.org/documentation/gstreamer/gstmessage.html#gst_message_parse_error).

## Linhas 160 a 172. Fim do fluxo e liberação da mensagem

```c
                case GST_MESSAGE_EOS:
                    g_print("Fim da configuracao %c.\n", 'A' + (int) i);
                    break;
                default:
                    g_printerr("Mensagem inesperada.\n");
                    resultado = 1;
                    break;
            }
            gst_message_unref(msg);
        } else {
            g_printerr("Nao foi recebida uma mensagem de encerramento.\n");
            resultado = 1;
        }
```

A linha 160 seleciona o caso de EOS, que significa End Of Stream, ou fim do fluxo. A linha 161 imprime a conclusão da configuração atual. A linha 162 sai do `switch`, mantendo `resultado` em 0.

As linhas 163 a 166 cobrem um tipo inesperado. O filtro pediu somente ERROR ou EOS, então esse caso não é esperado no caminho normal. O programa informa a situação, registra falha e sai do `switch`. A linha 167 fecha a seleção.

A linha 168 libera a referência à mensagem. O conteúdo necessário já foi lido; continuar guardando essa mensagem não ajuda a reprodução.

A linha 169 inicia o caso em que a espera não devolveu mensagem. A linha 170 informa a ausência, a linha 171 registra falha e a linha 172 encerra o `if`. Com espera ilimitada e uma execução normal, esperamos receber ERROR ou EOS. Um Bus em flushing é um exemplo de situação em que a espera pode retornar sem uma mensagem.

EOS é um evento que percorre o fluxo e resulta em mensagens de encerramento no Bus. A pipeline reúne o término de suas saídas antes de informar o fim à aplicação. Isso permite aguardar também a gravação do WAV, em vez de considerar somente o vídeo.

## Linhas 174 a 186. Encerramento da configuração e retorno

```c
        /* EOS permite que wavenc finalize o cabecalho dos arquivos. */
        gst_object_unref(bus);
        gst_element_set_state(data.pipeline, GST_STATE_NULL);
        gst_object_unref(data.video_queue);
        gst_object_unref(data.audio_queue);
        gst_object_unref(data.pipeline);

        if (resultado != 0)
            return resultado;
    }

    return 0;
}
```

A linha 174 explica o papel do EOS na gravação. `wavenc` precisa finalizar as informações do contêiner. Chegar ao fim normalmente permite completar o cabeçalho do arquivo. Uma saída interrompida por erro não recebe a mesma garantia de arquivo finalizado.

A linha 175 libera a referência ao Bus obtida na linha 141. A linha 176 coloca a pipeline em `GST_STATE_NULL`, interrompendo o processamento e encerrando seus recursos de execução.

As linhas 177 e 178 liberam as referências próprias às filas obtidas nas linhas 121 e 122. A linha 179 libera a pipeline, que possui os elementos do bin, incluindo a fonte. `gst_object_unref()` reduz a contagem de referências. O objeto é destruído quando não restam referências que o mantenham vivo.

Essas liberações têm funções distintas. `g_free()` libera uma string alocada, `gst_caps_unref()` libera uma referência de caps, `gst_message_unref()` libera uma mensagem e `gst_object_unref()` libera referências dos objetos GStreamer. Encerrar uma variável local ou retornar 1 não substitui essas chamadas.

A linha 181 verifica o resultado da execução. A linha 182 retorna a falha depois da limpeza. A linha 183 fecha o corpo do `for`. Se houve sucesso em A, o índice aumenta e toda a construção se repete em B. Se B teve sucesso, a condição do `for` deixa de ser atendida.

A linha 185 retorna 0 após as duas configurações. A linha 186 fecha `tutorial_main()`. O padrão de parada e limpeza veio do Tutorial 1 e as regras de referência estão em [GstObject](https://gstreamer.freedesktop.org/documentation/gstreamer/gstobject.html#gst_object_unref).

## Linhas 188 a 195. Entrada do callback de pad novo

```c
/* Adapta o callback do tutorial 3 para ligar tanto video quanto audio. */
static void pad_added_handler(GstElement *src, GstPad *new_pad, CustomData *data)
{
    GstCaps *new_pad_caps = gst_pad_get_current_caps(new_pad);
    GstPad *sink_pad = NULL;
    GstElement *entrada;
    GstPadLinkReturn ret;
    const gchar *new_pad_type;
```

A linha 188 registra a adaptação do Tutorial 3. A linha 189 começa a implementação declarada nas linhas 23 e 24, e a linha 190 abre seu corpo.

`src` é o elemento que emitiu `pad-added`, neste caso a fonte. `new_pad` é o pad que acabou de aparecer. `data` aponta para a estrutura cujo endereço foi passado em `g_signal_connect()`. Aqui usamos `data->campo` porque recebemos um ponteiro. Em `tutorial_main()`, usamos `data.campo` porque a variável é a estrutura em si.

A linha 191 consulta as caps atuais desse pad. A linha 192 inicia `sink_pad` como `NULL`; ainda precisamos escolher o destino. A linha 193 declara `entrada`, que apontará para a fila de áudio ou vídeo. A linha 194 declara o resultado da tentativa de ligação entre pads. A linha 195 declara o ponteiro para o nome do tipo de mídia.

O callback roda quando o pad é exposto, não uma vez por quadro. Um arquivo com vídeo e áudio pode gerar chamadas para os dois pads. Não dependemos da ordem em que eles aparecem, porque o tipo das caps decide o destino.

## Linhas 197 a 204. Consulta e validação das caps

```c
    if (new_pad_caps == NULL)
        new_pad_caps = gst_pad_query_caps(new_pad, NULL);
    if (new_pad_caps == NULL || gst_caps_is_empty(new_pad_caps) ||
        gst_caps_is_any(new_pad_caps)) {
        GST_ELEMENT_ERROR(src, CORE, NEGOTIATION,
                          ("Nao foi possivel identificar o novo pad."), (NULL));
        goto exit;
    }
```

A linha 197 verifica se a consulta inicial devolveu `NULL`, o que significa que ainda não há caps atuais. A linha 198 faz uma consulta das caps possíveis do pad. O segundo argumento `NULL` pede a consulta sem uma restrição adicional de formato.

As linhas 199 e 200 verificam três situações: não recebemos caps; o conjunto está vazio; ou o conjunto é ANY, sem um tipo específico para identificar. A operação seguinte precisa de uma estrutura concreta, então esses casos são tratados antes de acessá-la.

As linhas 201 e 202 usam `GST_ELEMENT_ERROR()` para publicar uma mensagem de erro no Bus. `src` identifica o elemento de origem. `CORE` e `NEGOTIATION` indicam domínio e código do erro. O primeiro grupo entre parênteses fornece a mensagem. O segundo grupo, aqui com `NULL`, não acrescenta texto de debug.

A linha 203 usa `goto exit` para seguir ao bloco de limpeza no fim do callback. A linha 204 fecha o `if`. Esse desvio não encerra o programa sozinho: ele encerra o trabalho do handler após liberar suas referências, e a thread da aplicação recebe a mensagem publicada no Bus.

A consulta e a identificação do tipo seguem o Tutorial 3. A verificação de caps ausentes ou sem um tipo utilizável é um cuidado acrescentado nesta adaptação. As funções estão em [GstCaps](https://gstreamer.freedesktop.org/documentation/gstreamer/gstcaps.html) e [GstPad](https://gstreamer.freedesktop.org/documentation/gstreamer/gstpad.html#gst_pad_get_current_caps).

## Linhas 206 a 212. Escolha do ramo pelo tipo de mídia

```c
    new_pad_type = gst_structure_get_name(gst_caps_get_structure(new_pad_caps, 0));
    if (g_str_has_prefix(new_pad_type, "video/x-raw"))
        entrada = data->video_queue;
    else if (g_str_has_prefix(new_pad_type, "audio/x-raw"))
        entrada = data->audio_queue;
    else
        goto exit;  /* Ignora legendas e outros tipos de fluxo. */
```

A linha 206 obtém a primeira estrutura das caps com índice 0 e extrai seu nome. No pad decodificado de vídeo, esperamos `video/x-raw`. No pad decodificado de áudio, esperamos `audio/x-raw`.

A linha 207 testa se o nome começa com `video/x-raw`, usando `g_str_has_prefix`. Quando isso ocorre, a linha 208 escolhe `data->video_queue` como destino.

A linha 209 faz o teste de áudio no `else if`. Quando ele é verdadeiro, a linha 210 escolhe `data->audio_queue`. As linhas 211 e 212 ignoram os outros tipos, encaminhando a execução à limpeza.

Esse é o acréscimo que permite usar a imagem e a trilha sonora do mesmo arquivo. O handler do Tutorial 3 original identifica áudio e ignora vídeo. A nossa escolha de destino aproveita o mesmo padrão para os dois tipos.

O nome de mídia obtido das caps pertence aos dados da estrutura. Guardamos esse ponteiro temporariamente enquanto `new_pad_caps` continua válido e não o liberamos com `g_free()`. Ele será usado antes do `gst_caps_unref()` da limpeza.

## Linhas 214 a 225. Ligação dos pads e resultado

```c
    sink_pad = gst_element_get_static_pad(entrada, "sink");
    if (gst_pad_is_linked(sink_pad))
        goto exit;  /* Usa somente a primeira trilha de cada tipo. */

    ret = gst_pad_link(new_pad, sink_pad);
    if (GST_PAD_LINK_FAILED(ret) && ret != GST_PAD_LINK_WAS_LINKED) {
        GST_ELEMENT_ERROR(src, CORE, NEGOTIATION,
                          ("Nao foi possivel ligar o fluxo %s.", new_pad_type),
                          ("gst_pad_link retornou %d", (int) ret));
    } else if (ret == GST_PAD_LINK_OK) {
        g_print("Pad dinamico: %s -> %s\n", new_pad_type, GST_ELEMENT_NAME(entrada));
    }
```

A linha 214 obtém o pad chamado `sink` da fila escolhida. Esse pad de entrada já existe na `queue`, por isso usamos `gst_element_get_static_pad()`. O novo pad dinâmico é o da fonte; o destino é um pad existente.

A linha 215 verifica se o destino já está ligado. A linha 216 vai à limpeza nesse caso. Cada fila recebe uma trilha. Assim, outras trilhas do mesmo tipo não são conectadas a esse destino. A seleção corresponde à primeira trilha que for ligada, sem um menu de escolha de faixas.

A linha 218 tenta a conexão na direção saída para entrada: `new_pad` da fonte e `sink_pad` da fila. `gst_pad_link()` liga pads existentes; não cria um novo filtro nem converte a mídia nessa operação.

A linha 219 verifica falha de ligação com `GST_PAD_LINK_FAILED(ret)`, mas separa `GST_PAD_LINK_WAS_LINKED`. Essa condição tolera o caso em que uma ligação já foi feita entre a checagem anterior e a tentativa. O operador `&&` exige que ambas as condições sejam verdadeiras para entrar no erro.

As linhas 220 a 222 publicam uma mensagem de erro real de ligação, incluindo o tipo de mídia e o código devolvido por `gst_pad_link`. A linha 223 identifica uma conexão feita com sucesso. A linha 224 imprime o tipo e o nome do destino; é a mensagem `Pad dinamico` vista no terminal. A linha 225 fecha o tratamento.

Agora existe um caminho que leva esse fluxo da fonte ao ramo preparado. Os buffers seguintes podem atravessá-lo sem que o programa precise ligar novamente cada quadro. A operação segue o Tutorial 3 e a referência [gst_pad_link](https://gstreamer.freedesktop.org/documentation/gstreamer/gstpad.html#gst_pad_link).

## Linhas 227 a 232. Limpeza comum do callback

```c
exit:
    if (new_pad_caps != NULL)
        gst_caps_unref(new_pad_caps);
    if (sink_pad != NULL)
        gst_object_unref(sink_pad);
}
```

A linha 227 define `exit`, um rótulo C. Os `goto exit` anteriores apontam para esse local. O nome não chama a função de encerramento do processo; é apenas o nome desse ponto dentro do handler.

A linha 228 verifica se há caps para liberar e a linha 229 libera sua referência. A linha 230 verifica se um pad de destino foi obtido e a linha 231 libera a referência desse pad. A linha 232 encerra a função.

O handler pode alcançar a limpeza após uma ligação, após ignorar uma trilha, ou após um erro. Centralizar essas chamadas evita deixar uma referência para trás em algum desses caminhos. Esse uso de um rótulo de limpeza também aparece no callback do Tutorial 3.

Não liberamos `src` nem `new_pad` aqui. Eles foram entregues pelo sinal para uso durante a chamada. As referências que precisamos devolver neste handler são as que obtivemos nas consultas de caps e do pad de destino. Liberar nossa referência a `sink_pad` também não desfaz uma ligação válida da pipeline.

## Linhas 234 a 248. Verificação dos fluxos após a descoberta

```c
/* Evita esperar por uma saida cujo fluxo nao existe no arquivo. */
static void no_more_pads_handler(GstElement *src, CustomData *data)
{
    GstPad *video_pad = gst_element_get_static_pad(data->video_queue, "sink");
    GstPad *audio_pad = gst_element_get_static_pad(data->audio_queue, "sink");
    gboolean tem_video = gst_pad_is_linked(video_pad);
    gboolean tem_audio = gst_pad_is_linked(audio_pad);

    gst_object_unref(video_pad);
    gst_object_unref(audio_pad);
    if (!tem_video || !tem_audio)
        GST_ELEMENT_ERROR(src, STREAM, FAILED,
                          ("O arquivo precisa conter uma trilha de video e uma de audio."),
                          ("Video ligado: %d; audio ligado: %d", tem_video, tem_audio));
}
```

A linha 234 registra a função desse segundo callback. A linha 235 implementa o handler declarado na linha 25. Ele recebe a fonte e o endereço de `CustomData`. A linha 236 abre o corpo.

A linha 237 obtém o pad de entrada da fila de vídeo e a linha 238 obtém o pad de entrada da fila de áudio. A linha 239 guarda se o destino de vídeo está ligado. A linha 240 faz o mesmo para áudio. `gboolean` é o tipo booleano usado pela GLib.

As linhas 242 e 243 liberam as duas referências aos pads depois de registrar o resultado. Precisamos apenas dos valores verdadeiro ou falso para a decisão seguinte.

A linha 244 verifica se falta uma das mídias. `!tem_video` significa que não houve ligação de vídeo. `!tem_audio` significa que não houve ligação de áudio. O `||` faz a condição valer quando qualquer uma delas faltar. Esse `!` é o operador lógico de C, fora de uma string; tem uma função diferente do separador `!` da descrição GStreamer.

As linhas 245 a 247 publicam erro de fluxo, com a mensagem sobre vídeo e áudio e os dois valores no debug. A linha 248 fecha o callback.

Essa guarda foi acrescentada para a arquitetura que preparamos. Já existem sinks aguardando os dois ramos. Se o arquivo tiver somente vídeo, uma saída de áudio sem fluxo pode impedir a preparação completa e deixar a espera por ERROR ou EOS parada. Quando `no-more-pads` informa que a fonte terminou de expor os pads, podemos reconhecer a ausência e encaminhar a falha ao Bus.

O sinal informa a conclusão da exposição dos pads, não o fim da reprodução do arquivo. A definição está em [GstElement, no-more-pads](https://gstreamer.freedesktop.org/documentation/gstreamer/gstelement.html#no-more-pads). Quando ambas as ligações existem, esse handler não publica erro e a reprodução continua.

## Linhas 250 a 258. Entrada do sistema e trecho da Apple

```c
/* Entrada especifica do macOS, preservada do tutorial. */
int main(int argc, char *argv[])
{
#if defined(__APPLE__) && TARGET_OS_MAC && !TARGET_OS_IPHONE
    return gst_macos_main ((GstMainFunc) tutorial_main, argc, argv, NULL);
#else
    return tutorial_main(argc, argv);
#endif
}
```

A linha 250 identifica a origem do trecho de plataforma. A linha 251 define `main()`, a função de entrada do programa C. A linha 252 abre seu corpo.

A linha 253 usa uma condição do pré-processador. Ela inclui a chamada específica da Apple quando o alvo é macOS e não iPhone. Essa decisão é feita durante a compilação. No Windows, a compilação usa o caminho do `#else`.

A linha 254 passa `tutorial_main` a `gst_macos_main`, a função de apoio usada pelos exemplos do GStreamer no ambiente macOS. O primeiro argumento é um ponteiro de função, convertido para `GstMainFunc`. `argc` e `argv` são os argumentos da aplicação. O último `NULL` não fornece dados adicionais ao helper. `return` devolve o resultado desse caminho ao sistema.

A função de apoio não está implementada em nosso `main.c`; ela pertence ao suporte fornecido pelo ambiente GStreamer usado no macOS. Conservamos a chamada do tutorial. O cast ajusta o tipo do ponteiro no local da chamada; ele não executa `tutorial_main` nessa expressão nem cria uma função nova.

A linha 255 inicia a alternativa de compilação. A linha 256 chama `tutorial_main(argc, argv)` diretamente. Esse é o caminho do Windows e do Linux. A linha 257 encerra a condição e a linha 258 fecha `main()`.

Manter essa entrada permite usar o mesmo arquivo C nas plataformas da demonstração. O trecho está nos exemplos oficiais dos Tutoriais 1, 2 e 3. As instruções de compilação para cada plataforma, inclusive `compilar_windows.bat`, estão no [README](README.md).

## O que demonstrar sobre PCM

A taxa de amostragem informa quantas amostras de cada canal representam um segundo. No formato A, são 48.000. Em B, são 8.000. A reamostragem mantém a duração do conteúdo e limita sua faixa de frequências. O limite teórico de Nyquist é metade da taxa: 24.000 Hz em A e 4.000 Hz em B. Aumentar a taxa de uma origem de baixa qualidade não recupera componentes que a origem já perdeu.

A profundidade informa a quantidade de valores de amplitude disponíveis. Um inteiro de 16 bits oferece 65.536 valores; um inteiro de 8 bits oferece 256. `S16LE` usa valores com sinal, enquanto `U8` usa valores sem sinal. A representação de 8 bits tem passos de amplitude maiores, aumentando o erro de quantização. O silêncio em S16LE fica em torno de zero; em U8, fica em torno do ponto médio, 128.

A quantidade de canais altera a representação espacial e o volume de dados. A conversão de uma gravação estéreo para mono reúne o conteúdo em um canal. Uma origem mono convertida para dois canais não passa a ter informação espacial originalmente gravada em estéreo. No exemplo incluído, os canais de origem têm o mesmo sinal; por isso, a diferença de canais aparece claramente no formato e tamanho do WAV, mas pode não ser percebida como uma mudança espacial.

Para PCM sem compressão, a taxa de dados depende de taxa de amostragem, bits por amostra e canais. A usa `48000 × 16 × 2 = 1536000` bits por segundo, ou 192.000 bytes. B usa `8000 × 8 × 1 = 64000` bits por segundo, ou 8.000 bytes. Para uma mesma duração, a proporção dos dados é aproximadamente 24 para 1. O cabeçalho e os metadados do WAV acrescentam bytes ao tamanho total dos arquivos.

Os WAVs permitem conferir essas propriedades mesmo quando o dispositivo adapta o áudio para reproduzi-lo. A conversão no ramo de reprodução ocorre depois do `tee`; o ramo de arquivo conserva as caps da experiência. O script opcional [verificar_pcm.py](verificar_pcm.py) lê os dois WAVs e compara os formatos, suas durações e a quantidade de dados. Ele não substitui a escuta nem comprova, sozinho, que a trilha de entrada foi processada até seu fim.

## Como relacionar o código aos requisitos

| Requisito da atividade | Onde aparece nesta implementação |
|---|---|
| C integrado ao GStreamer | Include da linha 9, `gst_init` e chamadas da biblioteca |
| Fonte de vídeo e fonte de áudio | Um arquivo com ambas as trilhas, aberto por `uridecodebin` e conectado nas linhas 206 a 218 |
| H.264/MPEG-4 AVC no vídeo | Encoder, parser e decoder nas linhas 34 a 36 |
| Processamento PCM | `audioconvert`, `audioresample` e caps nas linhas 49 a 51 e 85 a 90 |
| Pelo menos duas características PCM | Formato, taxa e canais mudam entre A e B |
| Pelo menos duas configurações | Array de configurações e repetição da linha 64 |
| Saídas funcionais | Janela de vídeo, reprodução de áudio e WAVs |
| Tratamento básico de erros | Falhas de URI, parser, estado, ligação e mensagens no Bus |
| Diagramas da solução | Arquivos Excalidraw, Draw.io e SVG na pasta `diagramas` |

Na apresentação, explique a ligação dinâmica apontando a linha 126 e depois o handler da linha 189. Mostre as caps do áudio nas linhas 50 e 51 e siga seu destino até a linha 89. Para a comparação visual, siga o `tee` da linha 38 até as entradas do compositor. Esses caminhos relacionam os conceitos às partes do código que realmente os executam.

## Referências para consulta

Além dos tutoriais associados no começo do guia, estas páginas documentam as APIs e elementos usados:

- [GstParse e gst_parse_launch](https://gstreamer.freedesktop.org/documentation/gstreamer/gstparse.html)
- [GstElement, estados, Bus, pads e sinais](https://gstreamer.freedesktop.org/documentation/gstreamer/gstelement.html)
- [GstBin, adição e busca por nome](https://gstreamer.freedesktop.org/documentation/gstreamer/gstbin.html)
- [GstPad, caps e ligação](https://gstreamer.freedesktop.org/documentation/gstreamer/gstpad.html)
- [GstCaps](https://gstreamer.freedesktop.org/documentation/gstreamer/gstcaps.html)
- [GstBus](https://gstreamer.freedesktop.org/documentation/gstreamer/gstbus.html)
- [GstMessage](https://gstreamer.freedesktop.org/documentation/gstreamer/gstmessage.html)
- [GstObject e contagem de referências](https://gstreamer.freedesktop.org/documentation/gstreamer/gstobject.html)
- [Conversão do caminho em URI](https://gstreamer.freedesktop.org/documentation/gstreamer/gsturihandler.html#gst_filename_to_uri)
- [uridecodebin](https://gstreamer.freedesktop.org/documentation/playback/uridecodebin.html)
- [Tipos de áudio bruto](https://gstreamer.freedesktop.org/documentation/additional/design/mediatype-audio-raw.html)
- [audioconvert](https://gstreamer.freedesktop.org/documentation/audioconvert/index.html) e [audioresample](https://gstreamer.freedesktop.org/documentation/audioresample/index.html)
- [x264enc](https://gstreamer.freedesktop.org/documentation/x264/index.html), [h264parse](https://gstreamer.freedesktop.org/documentation/videoparsersbad/h264parse.html) e [avdec_h264](https://gstreamer.freedesktop.org/documentation/libav/avdec_h264.html)
- [compositor](https://gstreamer.freedesktop.org/documentation/compositor/index.html) e [wavenc](https://gstreamer.freedesktop.org/documentation/wavenc/index.html)

As declarações e funções `g_` usadas na aplicação são da GLib e do GObject, que acompanham GStreamer. Para seus detalhes, consulte [g_strdup_printf](https://docs.gtk.org/glib/func.strdup_printf.html), [g_free](https://docs.gtk.org/glib/func.free.html), [g_clear_error](https://docs.gtk.org/glib/func.clear_error.html), [g_signal_connect](https://docs.gtk.org/gobject/func.signal_connect.html) e [g_object_set](https://docs.gtk.org/gobject/method.Object.set.html).
