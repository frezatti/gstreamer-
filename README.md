# gstreamer-

Aplicações em C para as atividades práticas de multimídia com GStreamer.

A atividade de **H.264/MPEG-4 e áudio PCM** está em [atividade_mpeg4_pcm](atividade_mpeg4_pcm/README.md).

- [Código-fonte completo](atividade_mpeg4_pcm/main.c)
- [Compilação, execução e demonstração](atividade_mpeg4_pcm/README.md)
- [Funcionamento e referências dos tutoriais](atividade_mpeg4_pcm/EXPLICACAO.md)
- [Diagramas editáveis no Draw.io](atividade_mpeg4_pcm/diagramas/pipelines.drawio)
- Diagramas editáveis no Excalidraw: [Vídeo](atividade_mpeg4_pcm/diagramas/video.excalidraw), [Áudio A](atividade_mpeg4_pcm/diagramas/audio_a.excalidraw) e [Áudio B](atividade_mpeg4_pcm/diagramas/audio_b.excalidraw).

A aplicação recebe um arquivo com vídeo e áudio, liga os fluxos dinamicamente com `uridecodebin` e executa duas configurações de PCM, gravando os resultados em WAV. Um [vídeo de exemplo](atividade_mpeg4_pcm/exemplo.mp4) está incluído para a demonstração.

Depois de compilar, execute `./atividade "video.mp4"`. Veja as instruções completas na pasta da atividade.
