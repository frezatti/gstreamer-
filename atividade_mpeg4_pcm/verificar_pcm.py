"""Confere os WAVs que a aplicacao realmente gerou. Usa apenas Python 3."""

from pathlib import Path
import sys
import wave


folder = Path(sys.argv[1]) if len(sys.argv) > 1 else Path.cwd()
data_sizes = []
durations = []

# A duracao depende do arquivo de entrada; A e B devem ter a mesma duracao.
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
        assert duration > 0, f"{filename}: audio vazio"
        samples = audio.readframes(audio.getnframes())
        assert len(samples) == audio.getnframes() * width * channels, f"{filename}: arquivo incompleto"
        data_sizes.append(len(samples))
        durations.append(duration)
        print(f"OK: {filename}: {rate} Hz, {width * 8} bits, "
              f"{channels} canal(is), {duration:.4f} s, "
              f"{len(samples)} bytes de PCM")

assert abs(durations[0] - durations[1]) < 0.01, "A e B tem duracoes diferentes"
# A taxa de dados e 24 vezes maior em A. Tolera uma amostra de arredondamento.
assert abs(data_sizes[0] - 24 * data_sizes[1]) <= 48, "Relacao de tamanhos inesperada"
print("OK: A usa aproximadamente 24 vezes mais dados PCM que B.")
