# The SnD bomb's pickup sound (owner 10-28, audio review: keypad 4, "a high quick chirp-chirp"): our own, synthesized.
# Two 3.4 kHz keypad chirps, 40 ms each, 45 ms apart, a little square for the piezo's edge, 4 ms ramps. 22050 Hz mono.
import wave, numpy as np, os, sys
sr = 22050
def tone(f, ms, amp=0.5, square=0.25):
    n = int(sr * ms / 1000); t = np.arange(n) / sr
    s = np.sin(2 * np.pi * f * t); s = (1 - square) * s + square * np.sign(s)
    a = np.ones(n); r = int(sr * 0.004); a[:r] = np.linspace(0, 1, r); a[-r:] = np.linspace(1, 0, r)
    return amp * s * a
def gap(ms): return np.zeros(int(sr * ms / 1000))
x = np.concatenate([tone(3400, 40), gap(45), tone(3400, 40), gap(150)])
out = sys.argv[1]
os.makedirs(os.path.dirname(out), exist_ok=True)
v = wave.open(out, 'wb'); v.setnchannels(1); v.setsampwidth(2); v.setframerate(sr)
v.writeframes((np.clip(x, -1, 1) * 32767 * 0.7).astype(np.int16).tobytes()); v.close()
print('pickup sound', out, len(x) / sr, 's')
