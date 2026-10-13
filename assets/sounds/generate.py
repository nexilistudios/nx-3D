"""Generate the original nx-3D gameplay sound effects.

Run from any directory: python3 assets/sounds/generate.py
Writes five utility clips and six distinct weapon shots next to this script.
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


def bomb_beep() -> list[float]:
    """Short, clean warning chirp repeated while the bomb is armed."""
    duration = 0.14
    result = []
    for index in range(int(duration * RATE)):
        t = index / RATE
        attack = min(1.0, t / 0.008)
        release = min(1.0, (duration - t) / 0.035)
        envelope = attack * release
        result.append(envelope * math.sin(2.0 * math.pi * 1050.0 * t))
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


def gunshot(seed: int, duration: float, body_pitch: float, crack: float,
            thump: float, tail: float, slide: float, slide_delay: float) -> list[float]:
    """Make a punchy report from muzzle noise, a short bass body, and action clack.

    The noise carries the shot; the low sweep adds a little game-style weight
    without turning the report into a pitched electronic sound.
    """
    rng = random.Random(seed)
    result = []
    fast_noise = 0.0
    mid_noise = 0.0
    low_noise = 0.0
    for index in range(int(duration * RATE)):
        t = index / RATE
        noise = rng.uniform(-1.0, 1.0)
        fast_noise += 0.38 * (noise - fast_noise)
        mid_noise += 0.075 * (noise - mid_noise)
        low_noise += 0.012 * (noise - low_noise)

        # High-frequency muzzle snap, then the broader pressure wave.
        muzzle = crack * (noise - mid_noise) * math.exp(-t * 105.0)
        pressure = (fast_noise - low_noise) * (0.65 + 0.35 * thump) * math.exp(-t * 32.0)
        bass = thump * math.sin(2.0 * math.pi *
                                (body_pitch * t - body_pitch * 1.6 * t * t)) * math.exp(-t * 46.0)
        decay = tail * mid_noise * math.exp(-t * 12.0)

        # The delayed bolt/slide is especially audible on the pistols.
        action = 0.0
        if t >= slide_delay:
            action_time = t - slide_delay
            action = slide * (0.7 * (noise - fast_noise) +
                              0.3 * math.sin(2.0 * math.pi * 1600.0 * action_time)) * \
                     math.exp(-action_time * 110.0)

        # Mild saturation gives the impact some arcade weight without clipping.
        result.append(math.tanh(1.4 * (muzzle + pressure + 0.36 * bass +
                                       1.5 * decay + action)))
    return result


def smoke() -> list[float]:
    """A soft canister pop followed by a pressurized hiss."""
    rng = random.Random(407)
    filtered = 0.0
    result = []
    for index in range(int(1.6 * RATE)):
        t = index / RATE
        noise = rng.uniform(-1.0, 1.0)
        filtered += 0.11 * (noise - filtered)
        pop = 0.9 * noise * math.exp(-t * 95.0)
        hiss = ((1.0 - math.exp(-t * 30.0)) * math.exp(-t * 1.65) *
                (0.33 * noise + 0.9 * filtered))
        result.append(pop + hiss)
    return result


if __name__ == "__main__":
    save("planting.wav", planting())
    save("bomb_beep.wav", bomb_beep())
    save("explosion.wav", explosion())
    save("reload.wav", reload())
    save("smoke.wav", smoke())
    for name, settings in {
        # Rifle reports are longer and deeper; the M16 has a brighter snap.
        "ak47": (401, 0.36, 112.0, 1.05, 1.10, 1.15, 0.24, 0.060),
        "m16": (402, 0.29, 145.0, 1.20, 0.78, 0.75, 0.22, 0.050),
        # Pistols end quickly, with a distinct slide cycling behind the crack.
        "glock": (403, 0.21, 185.0, 0.95, 0.48, 0.42, 0.44, 0.038),
        "usp": (404, 0.20, 165.0, 0.90, 0.55, 0.48, 0.50, 0.044),
        "deagle": (405, 0.43, 82.0, 1.25, 1.45, 1.35, 0.42, 0.070),
        # The MP5 has the shortest, tightest report for rapid fire.
        "mp5": (406, 0.17, 175.0, 0.78, 0.42, 0.38, 0.30, 0.033),
    }.items():
        save(f"{name}.wav", gunshot(*settings))
