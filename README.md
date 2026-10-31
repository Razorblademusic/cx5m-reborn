# CX5M Reborn — milestone 0.1

A **playable Windows VST3 starter project** for a four-operator FM instrument inspired by the Yamaha CX5M.

## Status and honesty about fidelity

This first milestone deliberately uses a **simple custom FM prototype**, NOT a Yamaha YM2151 emulator. It has four prototype algorithm arrangements, not the chip's eight actual algorithms, and simplified attack/release envelopes. Its purpose is to validate the JUCE VST3 shell, REAPER MIDI handling, parameter automation, UI and state persistence. **Do not use it to judge CX5M sound accuracy or interpret original cassette patches.**

Planned hardware-accurate implementation: evaluate available YM2151 cores, check source-code licenses, choose an appropriate implementation and map the original chip registers/patch parameters. The SFG-01/SFG-05 voice-data differences also require verification.

## Build on Windows

Requirements: Visual Studio 2022 with Desktop development with C++, CMake 3.22+, and a locally downloaded JUCE source tree (JUCE 7 or 8).

In PowerShell (adjust paths):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_DIR="C:/dev/JUCE"
cmake --build build --config Release
```

Find the resulting `CX5M Reborn.vst3` under `build/CX5MReborn_artefacts/Release/VST3/` and copy it into your VST3 folder (normally `C:\Program Files\Common Files\VST3`). Rescan VST3 plugins in REAPER.

This project has not been compiled or tested in a Windows toolchain here. It is source code, not a ready-to-install VST3 binary.

## Cassette WAV

The importer button is deliberately disabled until we have verified and implemented Yamaha's cassette bank encoding. Record an untouched stereo or mono WAV at 44.1/48 kHz, preferably 24-bit, without effects, and keep the original audio as an archive. Decoder development should first identify MSX cassette framing and then Yamaha's bank record structure. No assumption is made that all saved data is immediately compatible with standard MSX BASIC cassette records.

## Planned milestones

1. Integrate and validate licensed YM2151 core; complete eight algorithms and envelope/register mapping.
2. Create full hardware-parameter UI and voice/bank persistence with lossless raw-data preservation.
3. Decode actual cassette WAV and test recovered patches against reference material.

## Licensing

This original scaffold is provided for project development. JUCE and any future YM2151 core have separate licenses that must be reviewed before redistribution, especially for closed-source binaries.

## Cloud build (no studio-PC developer installation)

This archive includes `.github/workflows/windows-vst3.yml`.

1. Create a new **private** GitHub repository called `CX5M-Reborn` (do not initialise it with a README).
2. Upload the **contents of this project folder**, including the hidden `.github` folder, to the repository root. GitHub's web upload interface supports dragging the project files/folders from a local extracted folder; verify `.github/workflows/windows-vst3.yml` appears in the repository.
3. Open the repository's **Actions** tab and select **Build Windows VST3**, then **Run workflow**. A push to `main` also triggers a build.
4. When the run finishes, download `CX5M-Reborn-Windows-VST3` from the run's **Artifacts** section.
5. Extract the artifact and copy the `.vst3` bundle into `C:\Program Files\Common Files\VST3`, then rescan in REAPER.

**Build status:** This workflow has not yet been executed on GitHub. If it fails, share the error log so it can be corrected. It compiles the *prototype*, not an authentic YM2151 emulator, and does not yet decode cassette recordings. JUCE is fetched from its `master` branch; pin a reviewed release before making a reproducible public release. Review JUCE licensing before distribution.
