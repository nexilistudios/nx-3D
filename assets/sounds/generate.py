"""Generate the original nx-3D gameplay sound effects.

Run from any directory: python3 assets/sounds/generate.py
Only the three WAV files next to this script are written.
"""

from __future__ import annotations

import math
import random
import struct
import wave
from pathlib import Path


RATE = 44_100
DESTINATION = Path(__file__).resolve().parent


def save(name: str, samples: list[float]) -> None:
    peak = max(abs(sample) for sample in samples)
    gain = 0.88 / peak if peak else 0.0
    pcm = struct.pack(
        f"<{len(samples)}h",
        *(int(max(-1.0, min(1.0, sample * gain)) * 32767) for sample in samples),
    )
    with wave.open(str(DESTINATION / name), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(pcm)


def planting() -> list[float]:
    """Keypad chirps and small switch clicks across a three-second plant."""
    rng = random.Random(101)
    result = [0.0] * int(3.1 * RATE)
    presses = [0.04, 0.39, 0.77, 1.14, 1.52, 1.9, 2.28, 2.65]
    for index, start in enumerate(presses):
        base = int(start * RATE)
        frequency = [790, 1040, 910, 1210][index % 4]
        for offset in range(int(0.18 * RATE)):
            t = offset / RATE
            envelope = (1.0 - math.exp(-t * 180.0)) * math.exp(-t * 22.0)
            beep = math.sin(2.0 * math.pi * frequency * t)
            click = rng.uniform(-1.0, 1.0) * math.exp(-t * 115.0)
            result[base + offset] += 0.4 * envelope * beep + 0.2 * click
    base = int(2.82 * RATE)
    for offset in range(int(0.27 * RATE)):
        t = offset / RATE
        envelope = math.sin(math.pi * min(1.0, t / 0.27)) ** 2
        result[base + offset] += 0.32 * envelope * math.sin(
            2.0 * math.pi * (650.0 * t + 550.0 * t * t)
        )
    return result


def explosion() -> list[float]:
    """Sharp initial crack, descending bass impact, and fading noisy rumble."""
    rng = random.Random(202)
    result = []
    low_noise = 0.0
    for index in range(int(2.5 * RATE)):
        t = index / RATE
        noise = rng.uniform(-1.0, 1.0)
        low_noise += 0.012 * (noise - low_noise)
        crack = 0.85 * noise * math.exp(-t * 35.0)
        bass = 0.8 * math.sin(2.0 * math.pi * (78.0 * t - 12.0 * t * t)) * math.exp(-t * 4.0)
        rumble = 3.5 * low_noise * math.exp(-t * 1.8)
        hiss = 0.18 * noise * math.exp(-t * 7.0)
        result.append(crack + bass + rumble + hiss)
    return result


def reload() -> list[float]:
    """Magazine release, insertion, bolt pull, and a final metal click."""
    rng = random.Random(303)
    result = [0.0] * int(2.45 * RATE)
    for start, strength, pitch in [
        (0.04, 0.45, 1650),
        (0.48, 0.6, 1250),
        (1.28, 0.72, 900),
        (1.86, 0.55, 1900),
    ]:
        base = int(start * RATE)
        for offset in range(int(0.22 * RATE)):
            t = offset / RATE
            click = rng.uniform(-1.0, 1.0) * math.exp(-t * 90.0)
            ring = math.sin(2.0 * math.pi * pitch * t) * math.exp(-t * 38.0)
            result[base + offset] += strength * (0.65 * click + 0.35 * ring)
    filtered_noise = 0.0
    for index in range(int(0.78 * RATE)):
        t = index / RATE
        filtered_noise += 0.09 * (rng.uniform(-1.0, 1.0) - filtered_noise)
        envelope = math.sin(math.pi * t / 0.78) ** 2
        result[int(0.69 * RATE) + index] += 0.22 * envelope * filtered_noise
    return result


if __name__ == "__main__":
    save("planting.wav", planting())
    save("explosion.wav", explosion())
    save("reload.wav", reload())
