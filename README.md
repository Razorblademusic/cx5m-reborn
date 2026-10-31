# CX5M Reborn v0.3 — experimental YM2151 integration

This is a **source-code test build**, not a verified finished emulator. It replaces the v0.2 simplified FM oscillator with Aaron Giles' BSD-3-Clause **ymfm YM2151** emulation core. The chip emulation is real; the patch controls are **approximate mappings**, not original CX5M voice parameters. No cassette decoder is included yet.

## Building

Upload the contents of this directory to your GitHub repository, keeping the `.github/workflows/build-windows.yml` path. The workflow downloads JUCE and ymfm and compiles on Windows 2022. The GitHub Actions run produces an artifact if successful. No compiler is required on your studio PC.

## Caveats / test checklist

- This integration has **not been compiled or audio-tested** here; expect possible first-build fixes.
- The editor still has the prototype's ratio/level/attack/release controls. Algorithm choices are now 1–8.
- Velocity, bend, pitch, note release, envelope mappings, and host resampling need verification and refinement. Resampling is linear interpolation (not production-quality).
- Original YM2151 voice parameters, accurate CX5M SFG-05 software mapping, patch bank import and cassette WAV decoding are not yet implemented.
- Keep the v0.2 source/build artifact as a fallback.
- The ymfm source is fetched separately from https://github.com/aaronsgiles/ymfm and is licensed BSD-3-Clause. Preserve its copyright/license notice when redistributing binaries.
- This project uses JUCE; check JUCE licensing requirements before distribution.
