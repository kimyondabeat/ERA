"""Placeholder raboday kit for ERA (synthesized, no samples).
Writes Kit/<category>__NN__<Name>.flac. Replace these files with your own pack later:
same naming (category__NN__Name.flac), then rebuild.
Usage: python3 tools/make_placeholder_kit.py Kit
"""
import os, subprocess, sys, tempfile
import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, sosfilt

SR = 44100
rng = np.random.default_rng(7)

def t(sec): return np.arange(int(sec * SR)) / SR
def env_exp(sec, tau): return np.exp(-t(sec) / tau)
def bp(x, lo, hi, order=2): return sosfilt(butter(order, [lo, hi], 'bandpass', fs=SR, output='sos'), x)
def hp(x, f, order=2): return sosfilt(butter(order, f, 'highpass', fs=SR, output='sos'), x)
def lp(x, f, order=2): return sosfilt(butter(order, f, 'lowpass', fs=SR, output='sos'), x)
def noise(sec): return rng.standard_normal(int(sec * SR))
def sweep_sine(sec, f0, f1, tau):
    tt = t(sec); f = f1 + (f0 - f1) * np.exp(-tt / tau)
    return np.sin(2 * np.pi * np.cumsum(f) / SR)
def drive(x, amt): return np.tanh(x * amt) / np.tanh(amt)
def fade(x, ms=4):
    n = int(ms * SR / 1000); x = x.copy(); x[-n:] *= np.linspace(1, 0, n); return x
def norm(x, peak=0.89, max_rms=0.32):
    x = x / (np.max(np.abs(x)) + 1e-9) * peak
    r = np.sqrt(np.mean(x**2))
    return x * min(1.0, max_rms / r) if r > 0 else x
def mix(*parts):
    n = max(len(p) for p in parts); out = np.zeros(n)
    for p in parts: out[:len(p)] += p
    return out

def kick(f0, f1, dec, click, drv, dur=0.7):
    body = sweep_sine(dur, f0, f1, 0.035) * env_exp(dur, dec)
    c = hp(noise(0.012), 2500) * env_exp(0.012, 0.003) * click
    return drive(mix(body, c), drv)

def snare(tone, dec, nlo, nhi, ndec, crack=1.0, dur=0.45):
    tn = (np.sin(2*np.pi*tone*t(dur)) + 0.5*np.sin(2*np.pi*tone*1.6*t(dur))) * env_exp(dur, dec)
    nz = bp(noise(dur), nlo, nhi) * env_exp(dur, ndec) * 2.2
    c = hp(noise(0.006), 4000) * crack
    return drive(mix(tn * 0.7, nz, c), 1.6)

def rim(freqs, dec, nz=0.4, dur=0.18):
    s = sum(np.sin(2*np.pi*f*t(dur)) for f in freqs) * env_exp(dur, dec)
    return mix(s, hp(noise(0.02), 3000) * env_exp(0.02, 0.004) * nz)

def clap(n_hits, gap_ms, lo, hi, tail, dur=0.6):
    out = np.zeros(int(dur*SR))
    for i in range(n_hits):
        b = bp(noise(0.02), lo, hi) * env_exp(0.02, 0.004)
        s = int(i*gap_ms*SR/1000); out[s:s+len(b)] += b * (0.8 + 0.2*i/n_hits)
    tl = bp(noise(dur), lo*0.9, hi*1.2) * env_exp(dur, tail) * 0.8
    s = int((n_hits-1)*gap_ms*SR/1000)
    out[s:] += tl[:len(out)-s]
    return drive(out, 1.4)

def tom(f0, f1, dec, skin=0.3, dur=0.9):
    b = sweep_sine(dur, f0, f1, 0.08) * env_exp(dur, dec)
    b += 0.35 * sweep_sine(dur, f0*1.5, f1*1.5, 0.06) * env_exp(dur, dec*0.6)
    s = bp(noise(0.05), 800, 4000) * env_exp(0.05, 0.01) * skin
    return drive(mix(b, s), 1.3)

def graj(rate, dur, bright):
    tt = t(dur); g = (0.5 + 0.5*np.sign(np.sin(2*np.pi*rate*tt)))  # scrape teeth
    g = lp(g, 400)
    nz = bp(noise(dur), 2500, bright) * g
    e = np.minimum(1, tt/0.03) * np.exp(-np.maximum(0, tt-dur*0.6)/0.04)
    return nz * e

def shaker(dur, attack, lo):
    tt = t(dur); e = np.minimum(1, tt/attack) * np.exp(-tt/(dur*0.35))
    return hp(noise(dur), lo, 4) * e

def modal(freqs, decs, amps, dur, strike=0.2):
    tt = t(dur); out = sum(a*np.sin(2*np.pi*f*tt)*np.exp(-tt/d) for f, d, a in zip(freqs, decs, amps))
    return mix(out, hp(noise(0.01), 2000)*env_exp(0.01, 0.002)*strike)

def cowbell(f1, f2, dec, dur=0.5):
    tt = t(dur); sq = np.sign(np.sin(2*np.pi*f1*tt)) + np.sign(np.sin(2*np.pi*f2*tt))
    return bp(sq, 500, 3500) * np.exp(-tt/dec)

def voice(pitch0, pitch1, formants_a, formants_b, dur, glide=0.5, vib=5.0):
    """Very simple formant voice: glottal saw through 3 resonators, vowel glide a->b."""
    tt = t(dur)
    f = pitch1 + (pitch0 - pitch1) * np.exp(-tt / (dur*0.6))
    f = f * (1 + 0.012*np.sin(2*np.pi*vib*tt))
    ph = np.cumsum(f) / SR
    src = 2*(ph % 1) - 1 + 0.08*rng.standard_normal(len(tt))
    k = np.clip(tt/(dur*glide), 0, 1)
    out = np.zeros_like(tt)
    blk = 512
    for s in range(0, len(tt), blk):
        kk = k[s]; seg = src[max(0, s-2048):s+blk]
        y = np.zeros_like(seg)
        for (fa, ba), (fb, bb), gain in zip(formants_a, formants_b, (1.0, 0.6, 0.35)):
            fc = fa + (fb-fa)*kk; bw = ba + (bb-ba)*kk
            y += gain * bp(seg, max(60, fc-bw/2), fc+bw/2, 2)
        out[s:s+blk] = y[-min(blk, len(tt)-s):]
    e = np.minimum(1, tt/0.015) * np.exp(-np.maximum(0, tt-dur*0.55)/(dur*0.18))
    return drive(out*e*4, 1.5)

E = [(530,120),(1840,160),(2480,200)]; I = [(300,90),(2300,160),(3000,200)]
O = [(450,110),(800,120),(2830,200)]; A = [(750,130),(1220,140),(2600,200)]; U = [(320,90),(870,110),(2250,200)]

def siren(dur, lo, hi, rate):
    tt = t(dur); f = lo + (hi-lo)*(0.5+0.5*np.sin(2*np.pi*rate*tt - np.pi/2))
    s = np.sign(np.sin(2*np.pi*np.cumsum(f)/SR))
    return lp(s, 3500) * np.minimum(1, tt/0.05) * np.minimum(1, (dur-tt)/0.1)

def riser(dur):
    tt = t(dur); f = 120*np.power(30, tt/dur)
    saw = 2*((np.cumsum(f)/SR) % 1)-1
    nz = hp(noise(dur), 1500)
    e = (tt/dur)**2
    return lp(saw*0.6+nz*0.5, 9000)*e*np.minimum(1, (dur-tt)/0.02)

def laser(dur):
    return sweep_sine(dur, 3200, 120, 0.06) * env_exp(dur, 0.12)

def whistle(dur, f, trill):
    tt = t(dur); ff = f*(1+0.03*np.sign(np.sin(2*np.pi*trill*tt)))
    s = np.sin(2*np.pi*np.cumsum(ff)/SR) + 0.08*bp(noise(dur), f*0.8, f*1.3)
    return s*np.minimum(1, tt/0.01)*np.minimum(1, (dur-tt)/0.03)

def horn(dur, f):
    tt = t(dur)
    s = sum(2*((np.cumsum(np.full(len(tt), f*d))/SR) % 1)-1 for d in (1, 1.006, 0.994, 2.003))
    return lp(s, 2200)*np.minimum(1, tt/0.02)*np.minimum(1, (dur-tt)/0.04)

def impact(dur):
    return drive(mix(sweep_sine(dur, 120, 30, 0.2)*env_exp(dur, 0.5), lp(noise(dur), 1200)*env_exp(dur, 0.25)*0.7), 2.0)

KIT = {
 "kick": [("Kick (Bout)", kick(190, 48, 0.32, 0.6, 2.2)), ("Kick (Lari)", kick(230, 55, 0.22, 1.0, 3.0)),
          ("Kick (Gwo)", kick(160, 42, 0.55, 0.4, 1.8, 1.0)), ("Kick (Fo)", kick(260, 60, 0.18, 1.2, 4.0)),
          ("Kick (Dife)", kick(210, 50, 0.4, 0.8, 5.0)), ("Kick (Kanaval)", kick(175, 46, 0.45, 0.7, 2.8))],
 "snare": [("Snare (Kraze)", snare(200, 0.05, 1500, 8000, 0.09)), ("Snare (Lari)", snare(230, 0.04, 2000, 10000, 0.07, 1.4)),
           ("Snare (Bann a Pye)", snare(180, 0.07, 1000, 6000, 0.14)), ("Snare (Chale)", snare(210, 0.06, 1200, 7000, 0.11)),
           ("Snare (Dife)", snare(250, 0.035, 2500, 12000, 0.06, 1.8)), ("Snare (Kafou)", snare(190, 0.05, 900, 5000, 0.18))],
 "rim": [("Rim (Tak)", rim([820, 1650], 0.018)), ("Rim (Bwa)", rim([620, 1240], 0.03, 0.2)),
         ("Rim (Klak)", rim([1050, 2300], 0.012, 0.6)), ("Rim (Tchak)", rim([900, 1900, 3100], 0.02, 0.8)),
         ("Rim (Sek)", rim([1400], 0.008, 0.9)), ("Rim (Pik)", rim([700, 1500], 0.04, 0.3))],
 "percs": [("Percs (Graj)", graj(38, 0.32, 9000)), ("Percs (Graj Long)", graj(30, 0.6, 7000)),
           ("Percs (Chakchak)", shaker(0.22, 0.04, 5000)), ("Percs (Bwa)", modal([980, 2700], [0.05, 0.02], [1, 0.4], 0.3)),
           ("Percs (Kloch)", modal([540, 820, 1380, 2210], [0.5, 0.35, 0.25, 0.15], [1, 0.7, 0.5, 0.3], 1.0)),
           ("Percs (Ogan)", cowbell(545, 815, 0.12)), ("Percs (Tanbou Slap)", modal([330, 520, 760], [0.08, 0.05, 0.03], [1, 0.6, 0.4], 0.4, 0.6))],
 "vox": [("Vox (Hey!)", voice(260, 200, E, I, 0.42)), ("Vox (Ho!)", voice(220, 170, O, U, 0.4)),
         ("Vox (Ah)", voice(240, 230, A, A, 0.6)), ("Vox (Ye)", voice(300, 250, I, E, 0.38)),
         ("Vox (Wo-o)", voice(200, 260, U, O, 0.7, 0.7)), ("Vox (Chant)", np.concatenate([voice(230, 230, O, O, 0.3), voice(275, 275, E, E, 0.45)]))],
 "fx": [("FX (Siren)", siren(1.6, 650, 1350, 1.2)), ("FX (Riser)", riser(2.0)), ("FX (Laser)", laser(0.5)),
        ("FX (Fiyet)", whistle(0.6, 2650, 22)), ("FX (Klewon)", horn(0.7, 466)), ("FX (Impact)", impact(1.5))],
 "clap": [("Clap (Lari)", clap(4, 9, 900, 2600, 0.12)), ("Clap (Men)", clap(3, 11, 1100, 3000, 0.08)),
          ("Clap (Foul)", clap(6, 7, 800, 2400, 0.2)), ("Clap (Kout)", clap(3, 6, 1300, 3600, 0.05)),
          ("Clap (Gwo)", clap(5, 12, 700, 2000, 0.18)), ("Clap (Sek)", clap(2, 8, 1500, 4200, 0.04))],
 "tom": [("Tom (Gonaives)", tom(150, 105, 0.22)), ("Tom (Petyonvil)", tom(190, 140, 0.16)),
         ("Tom (Ba)", tom(110, 75, 0.3)), ("Tom (Wo)", tom(240, 180, 0.12)),
         ("Tom (Mitan)", tom(165, 120, 0.2, 0.5)), ("Tom (Fon)", tom(95, 62, 0.38, 0.2))],
}

out = sys.argv[1] if len(sys.argv) > 1 else "Kit"
os.makedirs(out, exist_ok=True)
for cat, items in KIT.items():
    for i, (name, x) in enumerate(items, 1):
        x = fade(norm(x))
        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
            wavfile.write(f.name, SR, x.astype(np.float32))
            dst = os.path.join(out, f"{cat}__{i:02d}__{name}.flac")
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", f.name, "-c:a", "flac", "-sample_fmt", "s32", "-bits_per_raw_sample", "24", dst], check=True)
            os.unlink(f.name)
print("done")
