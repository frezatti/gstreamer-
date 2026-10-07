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

int tutorial_main(int argc, char *argv[])
{
    /* Este fluxo de video sera usado nas duas configuracoes de audio. */
    const gchar *video =
        "queue name=entrada_video ! videoconvert ! videoscale ! videorate ! "
        "video/x-raw,format=I420,width=640,height=480,framerate=30/1,"
        "pixel-aspect-ratio=1/1 ! "
        "x264enc name=codificador tune=zerolatency "
        "speed-preset=ultrafast bitrate=1000 ! "
        "h264parse name=h264 ! avdec_h264 name=decodificador ! "
        "videoconvert ! video/x-raw,format=RGB ! "
        "tee name=v "
        "v. ! queue ! mix.sink_0 "
        "v. ! queue ! videoscale ! videorate ! videoconvert ! "
        "video/x-raw,format=GRAY8,width=320,height=240,framerate=10/1 ! "
        "mix.sink_1 "
        "compositor name=mix background=black "
        "sink_0::xpos=0 sink_1::xpos=640 sink_1::ypos=120 ! "
        "video/x-raw,width=960,height=480,framerate=30/1 ! "
        "videoconvert ! autovideosink ";

    /* Apenas estas caps mudam entre A e B. */
    const gchar *configuracoes[] = {
        "audio/x-raw,format=S16LE,rate=48000,channels=2,layout=interleaved",
        "audio/x-raw,format=U8,rate=8000,channels=1,layout=interleaved"
    };
    const gchar *arquivos[] = { "pcm_a.wav", "pcm_b.wav" };

    gst_init(&argc, &argv);

    if (argc != 2) {
        g_printerr("Uso: %s \"video.mp4\"\n", argv[0]);
        g_printerr("O arquivo deve conter video e audio.\n");
        return 1;
    }

    /* Executa o arquivo inteiro em A e depois abre o mesmo arquivo em B. */
    for (guint i = 0; i < G_N_ELEMENTS(configuracoes); i++) {
        CustomData data = { 0 };
        GstBus *bus;
        GstMessage *msg;
        GstStateChangeReturn ret;
        GError *error = NULL;
        int resultado = 0;
        gchar *uri = gst_filename_to_uri(argv[1], &error);

        if (uri == NULL) {
            g_printerr("Caminho invalido: %s\n", error->message);
            g_clear_error(&error);
            return 1;
        }

        /*
         * Prepara os dois ramos, ainda sem ligar a fonte.
         * O callback pad_added_handler fara essa ligacao, como no tutorial 3.
         */
        gchar *descricao = g_strdup_printf(
            "%s "
            "queue name=entrada_audio ! "
            "audioconvert ! audioresample ! %s ! "
            "tee name=a "
            "a. ! queue ! audioconvert ! audioresample ! autoaudiosink "
            "a. ! queue ! wavenc ! filesink location=%s",
            video, configuracoes[i], arquivos[i]);

        g_print("\nConfiguracao %c: %s\n", 'A' + (int) i, configuracoes[i]);
        g_print("Arquivo de entrada: %s.\n", argv[1]);
        g_print("Arquivo de saida: %s.\n", arquivos[i]);
        g_print("Video: H.264, com comparacao lado a lado.\n");

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
        data.video_queue = gst_bin_get_by_name(GST_BIN(data.pipeline), "entrada_video");
        data.audio_queue = gst_bin_get_by_name(GST_BIN(data.pipeline), "entrada_audio");
        g_object_set(data.source, "uri", uri, NULL);
        g_free(uri);

        g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler), &data);
        g_signal_connect(data.source, "no-more-pads", G_CALLBACK(no_more_pads_handler), &data);

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

        /* Aguarda um erro ou o fim de todos os fluxos da pipeline. */
        bus = gst_element_get_bus(data.pipeline);
        msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                        GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

        if (msg != NULL) {
            GError *err;
            gchar *debug_info;

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

/* Adapta o callback do tutorial 3 para ligar tanto video quanto audio. */
static void pad_added_handler(GstElement *src, GstPad *new_pad, CustomData *data)
{
    GstCaps *new_pad_caps = gst_pad_get_current_caps(new_pad);
    GstPad *sink_pad = NULL;
    GstElement *entrada;
    GstPadLinkReturn ret;
    const gchar *new_pad_type;

    if (new_pad_caps == NULL)
        new_pad_caps = gst_pad_query_caps(new_pad, NULL);
    if (new_pad_caps == NULL || gst_caps_is_empty(new_pad_caps) ||
        gst_caps_is_any(new_pad_caps)) {
        GST_ELEMENT_ERROR(src, CORE, NEGOTIATION,
                          ("Nao foi possivel identificar o novo pad."), (NULL));
        goto exit;
    }

    new_pad_type = gst_structure_get_name(gst_caps_get_structure(new_pad_caps, 0));
    if (g_str_has_prefix(new_pad_type, "video/x-raw"))
        entrada = data->video_queue;
    else if (g_str_has_prefix(new_pad_type, "audio/x-raw"))
        entrada = data->audio_queue;
    else
        goto exit;  /* Ignora legendas e outros tipos de fluxo. */

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

exit:
    if (new_pad_caps != NULL)
        gst_caps_unref(new_pad_caps);
    if (sink_pad != NULL)
        gst_object_unref(sink_pad);
}

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

/* Entrada especifica do macOS, preservada do tutorial. */
int main(int argc, char *argv[])
{
#if defined(__APPLE__) && TARGET_OS_MAC && !TARGET_OS_IPHONE
    return gst_macos_main ((GstMainFunc) tutorial_main, argc, argv, NULL);
#else
    return tutorial_main(argc, argv);
#endif
}
