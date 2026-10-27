# Contributing

Keep audio, drawing, presets, generators and tests in this repository. Implement
through the Open-XY public headers. Contributor callbacks contain no assembly,
raw firmware addresses, OS headers or platform conditionals. Audio callbacks must
not allocate, block, log, perform I/O or call graphics. Propose missing SDK
interfaces in a separate SDK pull request against `staging`.

Use the commands in README.md to run the SDK machine checks and
`OPEN_XY_SDK=../open-xy python tests/check.py`. `tests/source_contract.json` binds
the current machine's metadata and assets. The reference C++ tests retain
frozen audio, render-state and display fingerprints from the approved source;
changing expectations requires an intentional behavior change and independent
review. The historical hardware reference remains immutable outside this
repository.

The generators use NumPy 2.3.5. In a disposable source copy, run:

```sh
python generate_characters.py
python generate_bank.py
python pack_bank.py
python generate_panel.py
python generate_cast.py
```

`tests/bank_quality.py`, `tests/packing.py` and `tests/roster.py` independently
check spectral identity, exact sample decoding, data regeneration and artwork.
`author_predictors.py` is an optional offline authoring tool; normal regeneration
uses the committed integer coefficients in `predictors.json`.

The frozen fixture bindings recover the exact historical source bytes after
undoing only the `troupe` namespace rename. Keep those bindings and audio/display
golden values unchanged. `tests/cleanup_exact.cpp` compares 87,130,112 output
samples, 4,454,400 prepared samples and 8,192 display frames against the preceding
Cast renderer. Historical Ensemble workloads remain in `tests/legacy/`.

Before publishing source, audit the staged tree and complete Git history for
private firmware, archives, executable payloads, credentials and device records.
Keep source-only audit reports outside this checkout. Publishing source for
review is separate from tagging a hardware-qualified catalog release.

Preserve parameter IDs, ordering, meanings and defaults after publication.
Presets store normalized Voice values. Existing six-character Choir presets need
Voice reselected by name for the current ten-character ordering. Troupe's ID is
`troupe`; old `recital`/`choir` project mappings do not migrate automatically.
Do not replace an existing Recital or Choir layout entry silently.

Pin the compatible SDK revision in CI for each release. Native runtime behavior
must include the separate SDK display-activation reset change described in the
README. Before tagging a new revision for catalog publication, collect hardware
CPU measurements for that exact committed source. Do not publish private raw
captures, device metadata, firmware or firmware-containing packages. Hardware
installation and diagnostics require explicit authorization.
