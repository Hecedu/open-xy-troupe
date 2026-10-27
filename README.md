# Troupe

A vocal synthesizer for the OP-XY and [Open-XY SDK](https://github.com/etcetc000/open-xy).
Choose one of ten characters, morph its vowels, and blend in a cast of four
complementary voices. Five animated singers follow your performance on screen.
The voices are synthesized from authored waveforms; no recorded singers or
stock firmware are included.

## Controls and presets

| Control | Behavior |
| --- | --- |
| Vowel | Morphs continuously through five vowel anchors and matching mouths. |
| Voice | Selects Mandy, Bart, Zelda, Pip, Emily, Luis, Mabel, Rocco, Tamaki or Bertram. |
| Cast | Blends four complementary vocal characters into the lead; their figures fade in with the blend. |
| Effort | Changes vocal brightness and projection. |

Luis is the default voice. Twelve presets provide one starting point for each
character, plus **Low Oo** and **Solo Ah**. Notes have character-specific
attacks, delayed vibrato, independent drift and shaped aspiration. The SDK and
device own amplitude envelopes and note releases.

The five figures form a depth-scaled arc. Held notes drive sway and delayed eye
closure; the last release returns the pose to rest independently of the audio
tail. Character silhouettes dissolve over about 220 ms while their faces stay
aligned. Selecting a page or another track starts a 200 ms center-out entrance
when the SDK supplies display activation resets. Reduced motion settles the
formation immediately and removes sway.

## Review status

This repository is available for source review; it is not a catalog release.
The Troupe rename passed 13 C++ host tests, six machine-owned Python checks,
and browser ABI/render smoke checks using the preserved SDK described below.
Its controls, presets, authored audio data and animation match the preceding
Recital source except for the machine identity. Hardware stability and CPU
measurements for a committed Troupe revision remain pending.

As of October 27, 2026, two SDK dependencies must be resolved before a fresh
checkout can reproduce the full checks:

- The workflow pins SDK commit `1663f60d21c24bf7feafda28f01cd82efdf008d4`,
  based on staging `a1323c8` with a display-activation reset change. That commit
  is not currently available from `Hecedu/open-xy`, so GitHub Actions cannot
  check it out. The reset preserves entrances when reopening pages or switching
  tracks.
- The validated browser build used a fixed **1 MiB** WebAssembly memory
  allowance. The pinned SDK defaults to 512 KiB. The allowance was set by the
  local validation runner; it is not configured by this repository's workflow.
  Browser integration needs a supported SDK solution before CI can pass.

The preserved SDK targets OS 1.2.0. Current upstream targets OS 1.2.1;
compatibility with that revision has not been qualified. SDK fixes belong in
separate SDK pull requests. This repository contains the machine source only.

## Develop and audition

Use Python 3.11+, NumPy 2.3.5, Node 22.12+, a host C++11 compiler, and Clang with
`wasm-ld` for browser builds. On macOS, Homebrew LLVM supplies a suitable Clang;
Apple's compiler can run host tests but cannot link the WebAssembly preview.
Set `OPEN_XY_SDK` to the compatible SDK checkout, normally `../open-xy`.
Do not substitute another SDK revision when reproducing reference results.

Machine-owned data, artwork, native adapter and reference checks:

```sh
python -m pip install numpy==2.3.5
OPEN_XY_SDK=../open-xy python tests/check.py
```

Once the SDK dependencies above are resolved, run its complete machine checks
and build an audition into a fresh directory outside this repository:

```sh
export OPEN_XY_SDK=../open-xy
python "$OPEN_XY_SDK/sdk.py" test --machine . --cxx clang++ --host-cxx c++
python "$OPEN_XY_SDK/sdk.py" preview --machine . --out ../local/troupe-preview-001
python "$OPEN_XY_SDK/tools/serve_harness.py" --preview ../local/troupe-preview-001
```

Open the printed address, click **Enable audio**, and play the keyboard or
select a preset. See [CONTRIBUTING.md](CONTRIBUTING.md) for regeneration,
reference fixtures and validation requirements.

## Implementation and resources

The engine renders one oscillator per active note, blending the lead and Cast
banks during bounded waveform preparation. It supports 24 active note states,
256 native voice handles, and bounded voice-stealing tails. Audio callbacks
allocate no memory and perform no I/O, locking, FFTs or background work.

| Resource | Size |
| --- | --- |
| Lead bank | 1,450 waves × 256 samples; 227,308 bytes losslessly packed |
| Cast bank | 1,450 waves × 256 samples; 649,604 bytes in signed 14-bit direct lookup storage |
| Shared audio state | 82,448 bytes |
| Native voice handle | 16 bytes |
| Display state | 140 bytes; 156 nodes; no mutable paths |

These are authored-bank storage and declared state sizes, not a complete linked
module footprint or a device CPU measurement. Spectral sources, generators,
packing reports and exact reference tests are included.

## Identity and publication

Troupe's permanent ID and C++ namespace are `troupe`; the display name is
**Troupe**. Its requested engine/page are 36/84. Each firmware's layout assigns
and preserves actual numbers; always pass the previous build's `--layout`.
The `troupe` identity is separate from Recital and Choir: old `recital`/`choir`
presets, artwork names and project mappings do not migrate automatically. Keep
their existing firmware and layouts for older projects. Do not relabel a preserved
layout or reference manifest to make its old identity appear to be `troupe`.

Before publishing a catalog release, commit the machine and follow the SDK's
[hardware profiling guide](https://github.com/etcetc000/open-xy/blob/staging/docs/profiling.md):
build with profiling enabled, install only when authorized, collect three runs,
and publish the generated report for that exact commit. Host estimates do not
supply catalog CPU measurements. A maintainer can measure it if hardware is
unavailable. Reserve `troupe`, then submit only its entry in
`catalog/machines.json` against SDK `staging`, pinning a release tag, full commit,
repository path and generated CPU record.

Keep firmware, stock content and firmware-containing packages outside every
checkout. Source and preview checks do not establish device stability.

## Licence

Code, authored data and artwork are distributed under the [MIT licence](LICENSE).
Preserve the existing copyright and attribution notices when redistributing.
