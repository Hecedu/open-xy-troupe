#pragma once
#include <opxy/audio.hpp>
#include <opxy/lease_pool.hpp>
#include "voice_types.h"
#include "bank_stream.h"
namespace approved_cast {
using namespace troupe;
const unsigned max_singers=3, active_notes=24;
// No pointers in persistent state: identical layout in native and browser builds.
struct WaveKey { opxy::u32 type, pitch, pitch_mix, vowel, vowel_mix; };
struct RenderVoice {
    opxy::i16 tables[2][256];
    WaveKey current, target;
    BankStream decoding[4];
    opxy::u32 phase[3], random, initialized, active, building, cursor, fade, age, cached_build;
    opxy::i32 gains[3], lowpass, effort, ensemble;
    opxy::u32 vibrato_phase[3], drift_wait[3];
    opxy::i32 drift[3], drift_target[3], pitch_offset[3];
    opxy::u32 attack_phase, attack_step, vibrato_wait, vibrato_envelope, vibrato_fade_step;
    opxy::i32 vibrato_depth;
    opxy::i32 air_low, air_floor, air_gain;
    opxy::u32 entry_phase[2], entry_wait[2], entry_step[2], entry_mask;
};
// Per-stock-handle continuity survives eviction from the shared render pool.
// Samples are stored in the engine's i16 output units, never as a pointer to
// a render state that may already belong to a different note.
struct Voice {
    opxy::u32 lease;
    opxy::i32 last_sample, handoff_sample;
    opxy::u32 handoff_remaining;
};
// Fully decoded nearest-anchor waves survive note release. Thirty-two entries
// cover the complete 24-note pool plus turnover without per-track ownership.
// Tags are table IDs plus one; zero is an empty slot. No heap or pointers.
struct StartupCache {
    opxy::i16 waves[32][256];
    opxy::u32 tags[32], next;
};
// Completed, immutable pitch/vowel blends. Cache hits retain the original
// 16-block preparation schedule while bypassing repeated predictive decoding.
struct PreparedCache {
    opxy::i16 waves[64][256];
    WaveKey keys[64];
    opxy::u32 count, next;
};
struct Shared { opxy::LeasePool<RenderVoice,active_notes> voices; StartupCache startup; PreparedCache prepared; };
}
