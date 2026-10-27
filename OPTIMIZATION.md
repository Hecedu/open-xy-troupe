# Cast companion cleanup candidate

Base: hardware-approved `recital-cast-001`, commit
`87b7263f808f14ba1d7c1dab1425311c010f4892`, firmware SHA-256
`9ab506154869ff6b352ddc7b67747c806ca3224b27a5acc19c1b5c39adf561f3`.
The matching preserved SDK and Blackfin toolchain are unchanged.

The engine stores one singer per note and retains the 24-note render pool,
256 stock handles, stealing tails and both waveform caches. Removed companion
state includes their oscillator phases, gains, drift/vibrato trajectories,
pitch offsets and entrance envelopes. Unused Ensemble smoothing and companion
gain/tuning calculations are removed. Two companion drift timers and their
discarded PRNG draws remain: lead drift and aspiration share that random stream,
so preserving its exact sequence preserves the approved sound on long notes.

At exactly full Cast, waveform preparation reads and interpolates the authored
Cast bank directly, skipping the lead predictor and final identity blend.
Intermediate amounts retain the original integer blend order. Cold startup,
the 16-sample morph preparation budget and crossfade timing are unchanged.

RenderVoice shrinks from 1,400 to 1,320 bytes. Shared state shrinks from 84,368
to 82,448 bytes, saving 1,920 bytes across 24 note states. Only that resource
declaration changes in definition.json; controls, presets and identity are exact.
No bank compression, cache resizing or output-loop fusion is included.

Validation compares directly against a frozen, namespaced copy of the approved
Cast renderer and old state layout. Tests/frozen/bindings.json binds each fixture
back to the exact original bytes; historical audio and display golden values
are unchanged. Coverage includes 87,130,112 exact output samples, 4,454,400 exact
prepared samples and 8,192 byte-identical visual frames/states, all ten characters,
29 pitch anchors, fractional pitch/vowel blends, Cast endpoints and near-endpoints,
long/interrupted notes, PRNG continuity, cold/warm caches, 24-note playback,
25th-note stealing, sparse native handles and ASan/UBSan.

Display sources, artwork, Cast member identities, mouth/eye/sway/entrance behavior,
audio banks, generators and the selected layout remain unchanged. Browser
audition retains Cast 001's fixed 1 MiB reservation; native SDK sources are not
modified. Host timings do not establish target CPU improvement. The profiling
firmware candidate needs separate installation, hardware measurement and explicit
user stability confirmation before it can replace the approved Cast reference.
