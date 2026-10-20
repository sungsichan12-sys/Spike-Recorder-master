"""Small original drum one-shots; deterministic synthesis, no external assets."""
from array import array
from pathlib import Path
import math
import random
import wave

RATE = 22050
out = Path(__file__).with_name('assets')
for name, duration in [('snare', .23), ('tom', .40), ('ride', .65)]:
    rng = random.Random(1741)
    pcm = array('h')
    previous = 0.0
    for i in range(int(RATE * duration)):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        high = noise - previous
        previous = noise
        if name == 'snare':
            value = (.47 * high * math.exp(-20*t) +
                     .35 * math.sin(2*math.pi*185*t) * math.exp(-25*t))
        elif name == 'tom':
            phase = 2*math.pi*(108*t + 33*(1-math.exp(-18*t))/18)
            value = .75*math.sin(phase)*math.exp(-10*t) + .1*high*math.exp(-60*t)
        else:
            value = (.13*high + sum(.09*math.sin(2*math.pi*f*t)
                     for f in (1271, 1843, 2657, 3571, 4927))) * math.exp(-6*t)
        envelope = min(1, t/.001, (duration-t)/.012)
        pcm.append(round(max(-.95, min(.95, value * envelope)) * 32767))
    with wave.open(str(out / (name + '.wav')), 'wb') as f:
        f.setnchannels(1); f.setsampwidth(2); f.setframerate(RATE)
        f.writeframes(pcm.tobytes())
