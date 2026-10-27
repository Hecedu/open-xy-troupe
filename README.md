# Recital

Recital is a vocal ensemble synthesizer for the OP-XY and Open-XY SDK. Ten
selectable characters sing through a phase-coherent, synthetic vocal wave bank.
Five animated singers share the screen; each played note uses up to three audio
singers, with 24 active note states and bounded voice-stealing tails.

## Controls and presets

| Control | Behavior |
| --- | --- |
| Vowel | Morphs continuously through five authored vowel anchors and matching mouths. |
| Voice | Selects Mandy, Bart, Zelda, Pip, Emily, Luis, Mabel, Rocco, Tamaki or Bertram. |
| Ensemble | Brings in the audio companions and lights the four visual companions from the center outward. |
| Effort | Changes vocal brightness and projection. |

The twelve presets include one for each character, plus **Low Oo** and **Solo
Ah**. Luis remains the default voice. Character attacks, staggered companion
entrances, delayed vibrato, independent drift and shaped aspiration give notes
their vocal articulation. Stock amplitude envelopes and note releases remain
owned by the SDK and device.

The five figures form a depth-scaled arc. Held notes drive sway and delayed eye
closure; the last release returns the pose to rest independently of the audio
tail. Character silhouettes dissolve over about 220 ms while their faces stay
aligned. Selecting a page or another track starts a 200 ms center-out entrance
when the SDK supplies display activation resets. Reduced motion settles the
formation immediately and removes sway.

## Companion-sampling profiling candidate

This branch uses nearest-sample waveform lookup for the two audio companions;
the lead keeps linear interpolation. Both sides of companion crossfades use
the cheaper lookup. Quiet startup, upper-pitch fallback, expression, breath,
wave preparation, controls, presets and UI retain the decoder reference behavior.
Companions can sound grainier or more metallic, especially on bright high notes.
CPU benefit and acceptable sound quality require audition and hardware profiling.

The original decoder renderer and its historical fingerprints remain in tests.
A separate phase-quantized reference oracle checks current ensemble output,
exact solo output and unchanged control state without replacing old goldens.

## Develop and audition

Keep an [Open-XY SDK](https://github.com/etcetc000/open-xy) checkout beside this
repository as `../open-xy`, or set `OPEN_XY_SDK` to another checkout. Use the
exact revision pinned by [.github/workflows/machines.yml](.github/workflows/machines.yml):
`1663f60d21c24bf7feafda28f01cd82efdf008d4`, combining staging `a1323c8` with
our display-activation reset. That reset preserves the entrance when reopening
a native page or switching tracks; it is not yet in plain upstream staging.
Machine code uses public `<opxy/...>` headers; SDK changes belong in separate
SDK pull requests.

```sh
export OPEN_XY_SDK=../open-xy  # checkout at the pinned revision above
python "$OPEN_XY_SDK/sdk.py" test --machine . --cxx clang++ --host-cxx c++
python "$OPEN_XY_SDK/sdk.py" preview --machine . --out "$OPEN_XY_SDK/local/recital-preview-001"
python "$OPEN_XY_SDK/tools/serve_harness.py" --preview "$OPEN_XY_SDK/local/recital-preview-001"
```

Use Python 3.11+, Node 22.12+, NumPy 2.3.5 and LLVM with `wasm-ld`. On macOS,
pass Homebrew LLVM's `clang++` to `--cxx`; Apple's compiler can run host tests but
cannot link these WebAssembly previews. Open the printed localhost address,
click **Enable audio**, then play the keyboard or select a preset. Use a fresh
preview output directory after every source change.

`sdk.py test` runs all C++ sound, display and regression tests. Additional
machine-owned Python checks cover spectral quality, exact bank packing,
reproducible artwork and the generated native voice adapter:

```sh
python tests/check.py
```

Set `CXX` to a host C++11 compiler if needed. The workflow runs these checks on
Linux and compiles the native module with the pinned Windows Blackfin tools.
It never contacts a device. See [CONTRIBUTING.md](CONTRIBUTING.md) for changes
and regeneration.

## Build and publication

Follow the SDK's [build guide](https://github.com/etcetc000/open-xy/blob/staging/docs/build.md).
Recital's permanent ID and C++ namespace are `recital`; the display name is
**Recital**. Its requested engine/page are 36/84. Each firmware's layout assigns
and preserves actual numbers; always pass the previous build's `--layout`.
Recital is a new identity: old `choir` presets, artwork names and project mappings
do not migrate automatically. Keep existing Choir firmware and its layout for
older projects.

Before publishing, commit the machine and follow the SDK's
[hardware profiling guide](https://github.com/etcetc000/open-xy/blob/staging/docs/profiling.md):
build with profiling enabled, install only when authorized, collect three runs,
and publish the generated report for that exact commit. Host estimates do not
supply catalog CPU measurements. A maintainer can measure it if hardware is
unavailable. Reserve `recital`, then submit only its entry in
`catalog/machines.json` against SDK `staging`, pinning a release tag, full commit,
repository path and generated CPU record.

Keep firmware, stock content and firmware-containing packages outside every
checkout. Source and preview checks do not establish device stability.

## Data and licence

The 1,450 periodic waves are authored from synthetic harmonic magnitudes and
source phases. The repository contains no stock firmware, stock artwork or
sampled voices. Lossless predictive packing retains all 371,200 signed samples
and occupies 227,308 bytes. Rendering allocates no memory and performs no I/O,
locking or background work. Audio shared state is 84,368 bytes; each stock voice
handle uses 16 bytes. Display state is 140 bytes with 156 nodes and no mutable
paths.

Code, authored data and artwork are distributed under the [MIT licence](LICENSE).
Preserve the existing copyright and attribution notices when redistributing.
