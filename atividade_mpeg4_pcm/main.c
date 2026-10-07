/*
 * Adaptacao dos tutoriais basicos 1, 2, 6 e 10 do GStreamer.
 * Mantem a comparacao de video da atividade anterior.
 * Acrescenta H.264 e duas configuracoes de audio PCM.
 * Os exemplos e a documentacao dos elementos estao no README.md.
 */

#include <gst/gst.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

int tutorial_main(int argc, char *argv[])
{
    /* Este fluxo de video sera usado nas duas configuracoes de audio. */
    const gchar *video =
        "videotestsrc pattern=ball num-buffers=300 "
        "foreground-color=0xffff0000 background-color=0xff203060 ! "
        "video/x-raw,format=I420,width=640,height=480,framerate=30/1 ! "
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

    /* Executa A e depois B. Cada execucao dura aproximadamente 10 segundos. */
    for (guint i = 0; i < G_N_ELEMENTS(configuracoes); i++) {
        GstElement *pipeline;
        GstBus *bus;
        GstMessage *msg;
        GstStateChangeReturn ret;
        GError *error = NULL;
        int resultado = 0;

        /*
         * g_strdup_printf junta o video, as caps escolhidas e o nome do WAV.
         * Os dois fluxos pertencem a uma unica pipeline e tocam juntos.
         * 100 buffers x 4800 amostras / 48000 Hz = 10 segundos de audio.
         */
        gchar *descricao = g_strdup_printf(
            "%s "
            "audiotestsrc wave=saw freq=440 volume=0.2 "
            "num-buffers=100 samplesperbuffer=4800 ! "
            "audio/x-raw,format=F32LE,rate=48000,channels=2,layout=interleaved ! "
            "audioconvert ! audioresample ! %s ! "
            "tee name=a "
            "a. ! queue ! audioconvert ! audioresample ! autoaudiosink "
            "a. ! queue ! wavenc ! filesink location=%s",
            video, configuracoes[i], arquivos[i]);

        g_print("\nConfiguracao %c: %s\n", 'A' + (int) i, configuracoes[i]);
        g_print("Arquivo de saida: %s.\n", arquivos[i]);
        g_print("Video: H.264, com comparacao lado a lado.\n");

        /* Constroi a pipeline pelo texto, como no tutorial 1. */
        pipeline = gst_parse_launch(descricao, &error);
        g_free(descricao);
        if (error != NULL || pipeline == NULL) {
            g_printerr("Erro ao criar a pipeline: %s\n",
                       error != NULL ? error->message : "erro desconhecido");
            g_clear_error(&error);
            if (pipeline != NULL)
                gst_object_unref(pipeline);
            return 1;
        }

        /* Inicia a reproducao, seguindo o tutorial 2. */
        ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            g_printerr("Nao foi possivel iniciar a reproducao.\n");
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);
            return 1;
        }

        /* Aguarda um erro ou o fim de todos os fluxos da pipeline. */
        bus = gst_element_get_bus(pipeline);
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
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);

        if (resultado != 0)
            return resultado;
    }

    return 0;
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
