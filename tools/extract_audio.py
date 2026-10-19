#!/usr/bin/env python3
"""extract_audio.py - the game's sound, as files the VectorPie SDK plays.

From reference/tempest2k/src:
  sounds/samples/NN + incbin/sound_samples_table.bin -> assets/sounds/fxNN.wav
      NN is the effect number the game uses ('sfx'), 0-based
  sounds/*.mod -> assets/music/tuneN.wav, N the game's tune number ('modnum'):
      the part of the tune that repeats, and tuneN_intro.wav for what a tune
      plays once before that, if it has such a part

The modules are rendered by tools/mod2wav.c, which this builds with gcc.

    python tools/extract_audio.py
"""
import os
import shutil
import struct
import subprocess
import sys
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "reference", "tempest2k", "src")
ASSETS = os.path.join(ROOT, "assets")

# How fast the Jaguar's player (unused/syn6.o, taken apart) plays a sample.
# The DSP makes a sample every 2 * 32 * (SCLK + 1) clocks, SCLK = $1b, and
# moves on through the effect by its pitch in 256ths of a sample each time.
# PLAYFX2 makes that pitch from the number in the sample table (or the one a
# call gives instead): times 28, divided by 43. So the number is no period:
# the bigger, the higher.
OUTPUT_RATE = 26590906 / (2 * 32 * (0x1B + 1))


def dsp_pitch(number):
    return number * 28 // 43


def sample_rate(number):
    return OUTPUT_RATE * dsp_pitch(number) / 256

# images_sounds.s: the tunes in the order of the game's tune table.
TUNES = ["tune13", "tune7", "tune1", "tune3", "rave4", "tune5", "tune12"]


def effects():
    table = open(os.path.join(SRC, "incbin", "sound_samples_table.bin"), "rb").read()
    out = os.path.join(ASSETS, "sounds")
    os.makedirs(out, exist_ok=True)
    count = 0
    for i in range(30):
        row = table[i * 40:i * 40 + 40]
        name = row[:20].decode("latin-1").strip()
        period = struct.unpack(">H", row[22:24])[0]
        path = os.path.join(SRC, "sounds", "samples", "%02d" % (i + 1))
        data = open(path, "rb").read()
        # 8-bit signed in the ROM, 8-bit unsigned in a WAV file.
        with wave.open(os.path.join(out, "fx%02d.wav" % i), "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(1)
            w.setframerate(round(sample_rate(period)))
            w.writeframes(bytes((b + 128) & 255 for b in data))
        count += 1
        print("fx%02d  %-22s pitch %4d  %5d Hz  %6d bytes" % (i, name, period, round(sample_rate(period)), len(data)))
    return count


def music():
    """The tunes as they are: the game plays the modules itself (src/mod.c)."""
    out = os.path.join(ASSETS, "music")
    os.makedirs(out, exist_ok=True)
    for old in os.listdir(out):
        if old.endswith(".wav"):
            os.remove(os.path.join(out, old))
    for n, tune in enumerate(TUNES, 1):
        shutil.copyfile(os.path.join(SRC, "sounds", tune + ".mod"), os.path.join(out, "tune%d.mod" % n))


if __name__ == "__main__":
    effects()
    music()
    sys.exit(0)
