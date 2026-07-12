# Timbrine

A physics-driven timbre exploration tool that resynthesizes audio fragments and maps them into an interactive 2D timbre space.

This project is inspired by [Minuit Solstice](https://minuit.am/), a novel synthesizer VST that uses spectral analysis and timbre-based organization to extend and resynthesize small audio fragments from a user-provided sound file.

Audio is decomposed into short spectral frames, analyzed for timbral features, and embedded into a 2D timbre space where nearby points correspond to perceptually similar sounds. These fragments can be continuously extended and resynthesized, allowing navigation through timbre.

The project extends this idea by introducing physics-based interaction: sound fragments are chosen for playback by objects within simulated fields and forces, enabling dynamic, emergent playback behaviors driven by motion and interaction within the timbre map.

## Features

- [x] FFT/STFT-based spectral analysis and resynthesis
- [x] MFCC and spectral feature extraction (centroid, flux, roll-off)
- [x] 2D timbre embedding using dimensionality reduction
- [ ] Physics-based interaction and field-driven playback
- [x] Real-time audio playback and visualization

## Dependencies

- [miniaudio](https://miniaud.io/) — Cross-platform, single-header audio I/O and playback
- [pffft](https://github.com/marton78/pffft) — Lightweight SIMD-optimized FFT library for real-time spectral analysis
- [Qt Quick](https://doc.qt.io/qt-6/qtquick-index.html) — UI layer for real-time visualization and interaction
- [umappp](https://github.com/libscran/umappp) — C++ implementation of UMAP for timbre embedding
