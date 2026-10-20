"""Regenerate the original Night Pulse accompaniment (Python standard library)."""

from array import array
import math
import random
import wave
from pathlib import Path

RATE = 22050
DURATION = 35.5
BEAT = 0.5
SAMPLES = int(RATE * DURATION)
mix = array("f", [0.0]) * SAMPLES
rng = random.Random(20260928)


def add(start, duration, volume, sample):
    first = int(start * RATE)
    length = min(int(duration * RATE), SAMPLES - first)
    for i in range(max(0, length)):
        t = i / RATE
        mix[first + i] += volume * sample(t, i)


def kick(t, _):
    envelope = math.exp(-16 * t)
    phase = 2 * math.pi * (55 * t + 65 * (1 - math.exp(-15 * t)) / 15)
    return math.sin(phase) * envelope


def snare(t, _):
    return (rng.random() * 2 - 1) * math.exp(-20 * t) + \
           0.28 * math.sin(2 * math.pi * 180 * t) * math.exp(-24 * t)


def hat(t, _):
    return (rng.random() * 2 - 1) * math.exp(-52 * t)


for tick in range(int(DURATION / BEAT)):
    at = tick * BEAT
    if tick < 4:
        add(at, 0.07, 0.16 if tick < 3 else 0.26, hat)
        continue
    position = (tick - 4) % 4
    if position in (0, 2):
        add(at, 0.34, 0.35, kick)
    else:
        add(at, 0.23, 0.13, snare)
    add(at, 0.12, 0.06, hat)
    add(at + BEAT / 2, 0.09, 0.036, hat)

# A restrained melodic bass leaves the hit transients clear to the player.
roots = (110.0, 130.81, 146.83, 98.0)
for bar in range(16):
    root = roots[bar % len(roots)]
    for position in range(4):
        start = 2.0 + bar * 2.0 + position * BEAT
        frequency = root * (1.5 if position == 3 else 1.0)

        def bass(t, _, f=frequency):
            env = min(1.0, t * 45) * math.exp(-3.4 * t)
            return (math.sin(2 * math.pi * f * t) +
                    0.19 * math.sin(2 * math.pi * f * 2 * t)) * env

        add(start, 0.46, 0.11, bass)

out = Path(__file__).with_name("assets") / "night_pulse.wav"
out.parent.mkdir(exist_ok=True)
with wave.open(str(out), "wb") as wav:
    wav.setnchannels(1)
    wav.setsampwidth(2)
    wav.setframerate(RATE)
    pcm = array("h")
    for index, value in enumerate(mix):
        fade = min(1.0, index / (RATE * 0.05),
                   (SAMPLES - index) / (RATE * 0.75))
        pcm.append(int(max(-1.0, min(1.0, value * fade)) * 32767))
    wav.writeframes(pcm.tobytes())
print(out)
