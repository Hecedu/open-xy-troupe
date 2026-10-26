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

## Develop and audition

Keep an [Open-XY SDK](https://github.com/etcetc000/open-xy) checkout beside this
repository as `../open-xy`. Use its `staging` development branch and the compatible
revision pinned by `.github/workflows/machines.yml`. Machine code uses public
`<opxy/...>` headers; SDK changes belong in separate SDK pull requests.

```sh
python ../open-xy/sdk.py test --machine . --cxx clang++ --host-cxx c++
python ../open-xy/sdk.py preview --machine . --out ../open-xy/local/recital-preview-001
python ../open-xy/tools/serve_harness.py --preview ../open-xy/local/recital-preview-001
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
OPEN_XY_SDK=../open-xy python tests/check.py
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

Native re-entry animation requires the SDK display-activation reset contribution
prepared alongside this machine. Browser resets and track-change animation are
implemented through the public display callbacks. Use an SDK containing that
change to reproduce the complete approved entrance behavior.

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
