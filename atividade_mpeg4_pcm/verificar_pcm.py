"""Confere os WAVs que a aplicacao realmente gerou. Usa apenas Python 3."""

from pathlib import Path
import struct
import sys
import wave


folder = Path(sys.argv[1]) if len(sys.argv) > 1 else Path.cwd()
data_sizes = []

# Detecta falta de arquivo, formato errado, canais errados ou duracao incompleta.
for filename, rate, width, channels in [
    ("pcm_a.wav", 48000, 2, 2),
    ("pcm_b.wav", 8000, 1, 1),
]:
    path = folder / filename
    assert path.is_file(), f"Arquivo nao gerado: {path}"
    with wave.open(str(path), "rb") as audio:
        assert audio.getcomptype() == "NONE", f"{filename}: nao e PCM"
        assert audio.getframerate() == rate, f"{filename}: taxa incorreta"
        assert audio.getsampwidth() == width, f"{filename}: profundidade incorreta"
        assert audio.getnchannels() == channels, f"{filename}: canais incorretos"
        duration = audio.getnframes() / rate
        assert abs(duration - 10) < 0.01, f"{filename}: duracao {duration:.4f} s"
        samples = audio.readframes(audio.getnframes())
        if width == 2:
            # Primeiro canal de cada quadro estereo, em S16LE.
            values = {frame[0] for frame in struct.iter_unpack("<hh", samples)}
        else:
            values = set(samples)  # U8 mono: um byte por amostra.
        assert len(values) > 1, f"{filename}: audio sem variacao"
        data_sizes.append(len(samples))
        print(f"OK: {filename}: {rate} Hz, {width * 8} bits, "
              f"{channels} canal(is), {duration:.4f} s, "
              f"{len(samples)} bytes de PCM")

assert abs(data_sizes[0] / data_sizes[1] - 24) < 0.05, "Relacao de tamanhos inesperada"
print("OK: A usa aproximadamente 24 vezes mais dados PCM que B.")
