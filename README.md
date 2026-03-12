**A creative spectral wavefolder plugin** inspired by classic Buchla & Serge wavefolders, with extra modes for modern sound design.
<img width="1092" height="449" alt="image" src="https://github.com/user-attachments/assets/c1c91eee-cbc1-4db5-a0ae-11b942f60b46" />
## 🎛️ Overview

**Spectral Wavefolder** is a stereo wavefolding effect that turns simple waveforms into rich, harmonically dense tones. It ideally recreates Buchla-style parallel folding and Serge-style series folding, then adds five more original algorithms (Peak Clip, Center Fold, Fractal Fold, Wavewrap, and the brand-new **Tri Fold**).

Perfect for:
- Bass & lead synths
- Drums & percussion
- Guitar & vocal parallel processing
- Experimental sound design

## ✨ Features

### 7 Folding Modes
| # | Mode          | Description |
|---|---------------|-------------|
| 0 | **Buchla (parallel)** | Classic Buchla-style soft folding |
| 1 | **Serge (series)** | Multi-stage Serge cell + iterative folding |
| 2 | **Peak Clip** | Hard clipping at threshold |
| 3 | **Center Fold** | Folds around zero (removes center band) |
| 4 | **Fractal Fold** | 5-level recursive folding |
| 5 | **Wavewrap** | One-sided wrap-around fold |
| 6 | **Tri Fold** | Gentle piecewise-linear triangle-ish spectrum (new!) |

### Controls
- **Drive** (0.1–10×) – main fold intensity
- **Threshold** (0.05–1.0) – fold breakpoint
- **Mix** (0–100%) – wet/dry blend
- **Tilt** (±1) – Biasing darker or brighter spectra
- **Dynamics** (0–0.7) – RMS-based drive modulation (auto-creates punch)
- **Out Gain** (-24 to +12 dB)
- **Stereo Width** (-0.5 to +0.5) – M/S width control
- **Delta** toggle – outputs **only** the folded difference (great for parallel chains)

### Processing Highlights
- Full M/S encoding/decoding
- Real-time smoothed parameters
- DC blocking + soft limiter
- RMS envelope tracking
- Zero-latency (except plugin format overhead)
