#include "core.h"
#include "bank.h"
#include "effort.h"
#include "expression.h"
#include "cast_bank.h"
namespace troupe {
using opxy::u32;
using opxy::i32;
inline i32 bound(i32 x,i32 lo,i32 hi){return x<lo?lo:x>hi?hi:x;}
inline u32 random_word(u32& x){x^=x<<13;x^=x>>17;x^=x<<5;return x;}
inline i32 blend15(i32 a,i32 b,i32 t){return a+(b-a)*t/32768;}
inline bool same_key(const WaveKey& a,const WaveKey& b){return a.type==b.type&&a.pitch==b.pitch&&a.pitch_mix==b.pitch_mix&&a.vowel==b.vowel&&a.vowel_mix==b.vowel_mix;}
// Shape the path between authored vowels at block rate. This convex mapping
// preserves all five anchors and the phase-coherent bank's peak/DC bounds.
// Retain the 16-sample build budget and 128-sample fade; no live resonators.
inline i32 vowel_curve(i32 amount){
    if(amount==0||amount==32768)return amount;
    const i32 smooth=(amount*amount/32768)*(98304-2*amount)/32768;
    return amount+(smooth-amount)*7/20;
}
inline WaveKey key_for(opxy::Parameters parameters,u32 increment){
    // High bits encode a 1/256 Cast amount in the immutable cache key.
    WaveKey key={voice_type(parameters[1]) | (static_cast<u32>(effort_value(parameters[2])/128)<<8),0,0,0,0};
    const u32 coarse=increment>>9;
    // Fixed five comparisons for the 29 pitch anchors.
    u32 lo=0,hi=28;
    for(unsigned step=0;step<5;++step){
        const u32 middle=(lo+hi+1)/2;
        if(coarse>=(bank_pitch[middle]>>9))lo=middle;else hi=middle?middle-1:0;
    }
    key.pitch=lo;
    if(lo<28&&coarse>(bank_pitch[lo]>>9))
        key.pitch_mix=(coarse-(bank_pitch[lo]>>9))*256u/((bank_pitch[lo+1]>>9)-(bank_pitch[lo]>>9));
    const u32 position=static_cast<u32>(effort_value(parameters[0]))*4;
    key.vowel=position>>15;key.vowel_mix=vowel_curve(static_cast<i32>(position&32767));
    if(key.vowel>=4){key.vowel=4;key.vowel_mix=0;}
    return key;
}
inline unsigned raw_table(const WaveKey& key){return ((key.type&255)*29+key.pitch)*5+key.vowel;}
// Decode each pitch/vowel corner once for this 16-sample destination slice.
// Exact i32 blend order matches the original per-sample bank interpolation.
inline void lead_bank_slice(const WaveKey& key,unsigned start,opxy::i16* output,BankStream* streams){
    const unsigned base=(key.type*29+key.pitch)*5+key.vowel;
    opxy::i16 work[16],neighbor[16];
    bank_resume(base,start,output,streams[0]);
    if(key.vowel_mix){
        bank_resume(base+(key.vowel<4?1:0),start,work,streams[1]);
        for(unsigned i=0;i<16;++i)output[i]=static_cast<opxy::i16>(blend15(output[i],work[i],key.vowel_mix));
    }
    if(!key.pitch_mix)return;
    bank_resume(base+5,start,work,streams[2]);
    if(key.vowel_mix){
        bank_resume(base+5+(key.vowel<4?1:0),start,neighbor,streams[3]);
        for(unsigned i=0;i<16;++i)work[i]=static_cast<opxy::i16>(blend15(work[i],neighbor[i],key.vowel_mix));
    }
    for(unsigned i=0;i<16;++i)
        output[i]=static_cast<opxy::i16>(output[i]+(work[i]-output[i])*static_cast<i32>(key.pitch_mix)/256);
}
// Full Cast reads its exact authored mixture directly. At intermediate amounts,
// retain the approved lead decoding, interpolation and integer blend order.
template<bool Full>
inline void cast_bank_slice(const WaveKey& key,unsigned start,opxy::i16* output){
    const i32 amount=static_cast<i32>(key.type>>8)*128;
    const unsigned base=raw_table(key);
    for(unsigned i=0;i<16;++i){
        const unsigned at=start+i;
        i32 a=cast_sample(base,at);
        if(key.vowel_mix)a=blend15(a,cast_sample(base+(key.vowel<4?1:0),at),key.vowel_mix);
        if(key.pitch_mix){
            i32 b=cast_sample(base+5,at);
            if(key.vowel_mix)b=blend15(b,cast_sample(base+5+(key.vowel<4?1:0),at),key.vowel_mix);
            a+=(b-a)*static_cast<i32>(key.pitch_mix)/256;
        }
        output[i]=static_cast<opxy::i16>(Full?a:blend15(output[i],a,amount));
    }
}
inline void bank_slice(const WaveKey& key,unsigned start,opxy::i16* output,BankStream* streams){
    if((key.type>>8)==256){cast_bank_slice<true>(key,start,output);return;}
    WaveKey lead=key;lead.type&=255;
    lead_bank_slice(lead,start,output,streams);
    if(key.type>>8)cast_bank_slice<false>(key,start,output);
}
inline void begin_sound(RenderVoice& state,u32 owner){
    state.random=0x91e10da5u^(owner*0x9e3779b9u);if(!state.random)state.random=1;
    state.phase[0]=random_word(state.random);state.gains[0]=0;
    // Preserve the two removed phase draws before the lead's expression draws.
    random_word(state.random);random_word(state.random);
    state.initialized=state.building=state.cursor=state.fade=state.age=state.cached_build=0;
    state.active=2;state.lowpass=state.effort=0;
    state.attack_phase=state.attack_step=state.vibrato_wait=state.vibrato_envelope=state.vibrato_fade_step=0;
    state.vibrato_depth=0;state.air_low=state.air_floor=state.air_gain=0;
    state.vibrato_phase[0]=random_word(state.random);
    state.drift_wait[0]=128+(random_word(state.random)&255);
    state.drift[0]=state.drift_target[0]=state.pitch_offset[0]=0;
    for(unsigned j=0;j<2;++j){
        random_word(state.random); // Removed companion vibrato-phase seed.
        state.companion_drift_wait[j]=128+(random_word(state.random)&255);
    }
}
inline i32 sample(const opxy::i16* table,u32 phase){
    const u32 index=phase>>24;
    const i32 a=table[index],b=table[(index+1)&255];
    return a+(b-a)*static_cast<i32>((phase>>16)&255)/256;
}
// Identical steady-table oscillator with block choices specialized once.
template<bool Unity>
inline void steady_oscillator(i32* mixed, const opxy::i16* table, u32& phase,
                              i32 gain, i32 gain_step, i32 offset,
                              i32 offset_step, opxy::Pitch pitch) {
    for (unsigned i=0; i<64; ++i) {
        gain += gain_step;
        const i32 value = sample(table, phase);
        const i32 contribution = Unity ? value : value*gain/32768;
        offset += offset_step;
        phase += pitch[i] + static_cast<u32>(offset);
        mixed[i] = pitch[i]>bank_pitch[28] ? sample(sine, phase)/4 : contribution;
    }
}
inline bool cached_wave(StartupCache* cache,unsigned table,opxy::i16* output){
    if(!cache)return false;
    for(unsigned slot=0;slot<32;++slot)if(cache->tags[slot]==table+1){
        for(unsigned i=0;i<256;++i)output[i]=cache->waves[slot][i];
        return true;
    }
    return false;
}
inline void remember_wave(StartupCache* cache,unsigned table,const opxy::i16* input){
    if(!cache)return;
    // Another voice may have finished this same anchor in the meantime.
    for(unsigned slot=0;slot<32;++slot)if(cache->tags[slot]==table+1)return;
    const unsigned slot=cache->next;cache->next=(slot+1)&31;
    for(unsigned i=0;i<256;++i)cache->waves[slot][i]=input[i];
    cache->tags[slot]=table+1;
}
inline bool cached_prepared(PreparedCache* cache,const WaveKey& key,opxy::i16* output){
    if(!cache)return false;
    for(unsigned slot=0;slot<cache->count;++slot)if(same_key(cache->keys[slot],key)){
        for(unsigned i=0;i<256;++i)output[i]=cache->waves[slot][i];
        return true;
    }
    return false;
}
inline void remember_prepared(PreparedCache* cache,const WaveKey& key,const opxy::i16* input){
    if(!cache)return;
    for(unsigned slot=0;slot<cache->count;++slot)if(same_key(cache->keys[slot],key))return;
    const unsigned slot=cache->next;cache->next=(slot+1)&63;
    for(unsigned i=0;i<256;++i)cache->waves[slot][i]=input[i];
    cache->keys[slot]=key;
    if(cache->count<64)++cache->count;
}
// Exact signed division by ten for the sine interpolator's [-32767,32767]
// range. Its positive reciprocal product fits i32. The pinned native compiler
// otherwise calls software division for every cold-start oscillator sample.
inline i32 quiet_fundamental(i32 value){
    const i32 magnitude=value<0?-value:value;
    const i32 tenth=(magnitude*26215)/262144;
    return value<0?-tenth:tenth;
}
// Preserve the approved linear crossfade, quiet startup and upper fallback.
inline void transition_oscillator(i32* mixed,const opxy::i16* from,const opxy::i16* to,u32& phase,
                                 i32 gain,i32 gain_step,i32 offset,i32 offset_step,opxy::Pitch pitch,
                                 bool fading,u32 fade,bool unity){
    for(unsigned i=0;i<64;++i){
        gain+=gain_step;
        i32 value=from?sample(from,phase):quiet_fundamental(sample(sine,phase));
        if(fading)value=blend15(value,sample(to,phase),static_cast<i32>(fade+i)*256);
        const i32 contribution=unity?value:value*gain/32768;
        offset+=offset_step;
        phase+=pitch[i]+static_cast<u32>(offset);
        mixed[i]=pitch[i]>bank_pitch[28]?sample(sine,phase)/4:contribution;
    }
}
// A cold cache miss resumes only 64 source samples per callback. A quiet
// fundamental carries the existing onset until that wave can fade in. Warm
// notes copy one cached wave; subsequent morphs retain their 16-sample budget.
inline void update_table(RenderVoice& state,const WaveKey& wanted,StartupCache* cache=nullptr,PreparedCache* prepared=nullptr){
    if(!state.initialized){
        state.current=wanted;state.current.type&=255;state.current.pitch_mix=state.current.vowel_mix=0;
        state.initialized=1;
        const unsigned table=raw_table(state.current);
        if(cached_wave(cache,table,state.tables[0]))state.active=0;
        else{
            state.target=state.current;state.cursor=0;state.building=1;state.active=2;
        }
    }
    if(!state.building&&!state.fade&&!same_key(state.current,wanted)){
        state.target=wanted;state.cursor=0;state.building=1;
        const unsigned destination=state.active==0?1:0;
        state.cached_build=cached_prepared(prepared,state.target,state.tables[destination])?1:0;
    }
    if(state.building){
        const u32 destination=state.active==0?1:0;
        if(state.active==2){
            bank_dispatch(raw_table(state.target),state.cursor,64,state.tables[destination]+state.cursor,&state.decoding[0]);
            state.cursor+=64;
        }else{
            if(!state.cached_build)bank_slice(state.target,state.cursor,state.tables[destination]+state.cursor,state.decoding);
            state.cursor+=16;
        }
        if(state.cursor==256){
            state.building=0;state.fade=1;
            if(state.active==2)remember_wave(cache,raw_table(state.target),state.tables[destination]);
            else if(!state.cached_build)remember_prepared(prepared,state.target,state.tables[destination]);
        }
    }
}
inline i32 attack_curve(u32 phase){
    const i32 t=static_cast<i32>(phase);
    return (t*t/32768)*(98304-2*t)/32768;
}
inline i32 air_envelope(u32 phase){
    const i32 lead=attack_curve(phase<8192?phase*4:32768);
    return lead*(49152-attack_curve(phase)/2)/32768;
}
// Products stay within signed 32 bits even for the full unsigned pitch input.
inline i32 pitch_delta(u32 pitch,i32 tuning){
    return static_cast<i32>(pitch>>16)*tuning+static_cast<i32>(pitch&65535)*tuning/65536;
}
// Only the audible lead needs drift smoothing, vibrato lookup and tuning.
// Keep the removed companions' random draw times so lead drift and aspiration
// remain sample-exact, including long notes and interrupted control changes.
inline i32 update_expression(RenderVoice& state,const CharacterExpression& character){
    if(state.vibrato_wait)--state.vibrato_wait;
    else state.vibrato_envelope=static_cast<u32>(bound(static_cast<i32>(state.vibrato_envelope+state.vibrato_fade_step),0,32768));
    state.vibrato_depth+=(character.vibrato_depth-state.vibrato_depth)/8;
    const i32 depth=state.vibrato_depth*static_cast<i32>(state.vibrato_envelope)/32768;
    if(state.drift_wait[0])--state.drift_wait[0];
    else{
        state.drift_target[0]=(static_cast<i32>(random_word(state.random)&127)-64)*256;
        state.drift_wait[0]=256+(random_word(state.random)&255);
    }
    state.drift[0]+=(state.drift_target[0]-state.drift[0])/128;
    const i32 vibrato=depth?sample(sine,state.vibrato_phase[0])*depth/32768:0;
    const i32 target=vibrato+state.drift[0]/256;
    state.vibrato_phase[0]+=character.vibrato_rate;
    for(unsigned j=0;j<2;++j){
        if(state.companion_drift_wait[j])--state.companion_drift_wait[j];
        else{
            random_word(state.random); // Removed companion drift target.
            state.companion_drift_wait[j]=256+(random_word(state.random)&255);
        }
    }
    return target;
}
// Completed onsets and unchanged output level are common in release tails.
// Select that exact identity case once per block, keeping its sample loop
// free of redundant envelope products and volume-ramp arithmetic.
template<bool Settled>
inline void output_pass(RenderVoice& state,opxy::Output output,opxy::Pitch pitch,const i32* mixed,i32 brightness,i32 air,
                        i32 volume_start,i32 volume_goal,i32 onset,i32 onset_step,i32 breath_onset,i32 breath_step){
    u32 random=state.random;
    i32 air_low=state.air_low,air_floor=state.air_floor,lowpass=state.lowpass;
    i32 volume_numerator=0;
    const i32 volume_delta=volume_goal-volume_start;
    for(unsigned i=0;i<64;++i){
        if(!Settled){volume_numerator+=volume_delta;onset+=onset_step;breath_onset+=breath_step;}
        const i32 volume=Settled?volume_start:volume_start+volume_numerator/64;
        const i32 white=(static_cast<i32>(random_word(random)&255)-128)*64;
        air_low+=(white-air_low)/2;
        air_floor+=(air_low-air_floor)/8;
        const i32 noise=(air_low-air_floor)*air/8192;
        lowpass+=(mixed[i]-lowpass)*brightness/32768;
        const i32 voiced=Settled?lowpass:lowpass*onset/32768;
        const i32 aspiration=Settled?noise:noise*breath_onset/32768;
        const i32 value=(voiced+aspiration)*volume/32768;
        output[i]=(!pitch[i]||pitch[i]>=0x7c000000u)?0:bound(value,-12288,12288)*65536;
    }
    state.random=random;state.air_low=air_low;state.air_floor=air_floor;state.lowpass=lowpass;
}
inline void render_sound(RenderVoice& state,opxy::Output output,opxy::Parameters parameters,opxy::Pitch pitch,StartupCache* cache=nullptr,PreparedCache* prepared=nullptr){
    const WaveKey wanted=key_for(parameters,pitch[0]);
    const CharacterExpression& character=character_expression[wanted.type&255];
    const bool fresh=!state.initialized;
    if(fresh){
        state.attack_step=character.attack_step;
        state.vibrato_wait=character.vibrato_delay;
        state.vibrato_fade_step=character.vibrato_fade_step;
        state.vibrato_depth=character.vibrato_depth;
    }
    update_table(state,wanted,cache,prepared);
    const opxy::i16* from=state.active<2?state.tables[state.active]:nullptr;
    const u32 destination=state.active==0?1:0;
    const opxy::i16* to=state.tables[destination];
    const bool fading=state.fade!=0;
    const i32 previous_effort=state.effort;
    const i32 gain=state.gains[0],gain_step=(32768-gain)/64;
    state.effort+=(effort_value(parameters[3])-state.effort)/8;
    // Keep the authored Effort response while preventing its fixed low-pass
    // corner from swallowing upper-register fundamentals. Unity is stable.
    const i32 brightness=bound(10000+state.effort*22000/32768,
                               static_cast<i32>(pitch[0]>>12)>32768?32768:static_cast<i32>(pitch[0]>>12),32768);
    if(fresh)state.air_gain=character_air[wanted.type&255];
    else state.air_gain+=(character_air[wanted.type&255]-state.air_gain)/8;
    const i32 air=(32768-state.effort)/512*state.air_gain/256;
    // Preserve the approved solo level; no extra-layer compensation.
    const i32 level=46341;
    const i32 volume_goal=(26000+state.effort/4)*level/32768;
    const i32 volume_start=(26000+previous_effort/4)*46341/32768;
    const i32 offset_goal=pitch_delta(pitch[0],update_expression(state,character));
    const i32 offset=fresh?offset_goal:state.pitch_offset[0];
    const i32 offset_step=(offset_goal-offset)/64;
    const u32 next_attack=static_cast<u32>(bound(static_cast<i32>(state.attack_phase+state.attack_step),0,32768));
    i32 onset=attack_curve(state.attack_phase);
    const i32 onset_step=(attack_curve(next_attack)-onset)/64;
    i32 breath_onset=air_envelope(state.attack_phase);
    const i32 breath_step=(air_envelope(next_attack)-breath_onset)/64;
    i32 mixed[64];
    u32 phase=state.phase[0];
    const bool unity=gain==32768&&gain_step==0;
    if(from&&!fading){
        if(unity)steady_oscillator<true>(mixed,from,phase,gain,gain_step,offset,offset_step,pitch);
        else steady_oscillator<false>(mixed,from,phase,gain,gain_step,offset,offset_step,pitch);
    }else transition_oscillator(mixed,from,to,phase,gain,gain_step,offset,offset_step,pitch,fading,state.fade,unity);
    state.phase[0]=phase;
    if(state.attack_phase==32768&&volume_start==volume_goal)
        output_pass<true>(state,output,pitch,mixed,brightness,air,volume_start,volume_goal,onset,onset_step,breath_onset,breath_step);
    else output_pass<false>(state,output,pitch,mixed,brightness,air,volume_start,volume_goal,onset,onset_step,breath_onset,breath_step);
    state.attack_phase=next_attack;
    if(state.age<0x7fffffbfu)state.age+=64;
    state.pitch_offset[0]=offset_goal;state.gains[0]=32768;
    if(fading){
        state.fade+=64;
        if(state.fade>=129){state.active=destination;state.current=state.target;state.fade=0;}
    }
}
inline void destroy(Voice& voice,Shared& shared,u32 owner){
    shared.voices.release(owner,voice.lease);voice.lease=0;
    voice.last_sample=voice.handoff_sample=0;voice.handoff_remaining=0;
}
inline void initialize(Voice& voice,Shared& shared,u32 owner){
    destroy(voice,shared,owner);voice.lease=shared.voices.acquire(owner,0);
    RenderVoice* state=shared.voices.get(owner,voice.lease);if(state)begin_sound(*state,owner);
}
inline void start(Voice& voice,Shared& shared,u32 owner){
    // Stock owns note retrigger/envelope timing. A new note retains its exact
    // original vocal onset, including when this handle used to carry a tail.
    voice.last_sample=voice.handoff_sample=0;voice.handoff_remaining=0;
    voice.lease=shared.voices.acquire(owner,voice.lease);
    RenderVoice* state=shared.voices.get(owner,voice.lease);if(state)begin_sound(*state,owner);
}
inline void render(Voice& voice,Shared& shared,u32 owner,opxy::Output output,opxy::Parameters parameters,opxy::Pitch pitch){
    RenderVoice* state=shared.voices.get(owner,voice.lease);
    if(state)render_sound(*state,output,parameters,pitch,&shared.startup,&shared.prepared);
    else{
        if(voice.lease){
            voice.handoff_sample=voice.last_sample;
            voice.handoff_remaining=voice.last_sample?128:0;
            voice.lease=0;
        }
        for(unsigned i=0;i<64;++i)output[i]=0;
    }
    if(voice.handoff_remaining){
        for(unsigned i=0;i<64;++i){
            const i32 remaining=static_cast<i32>(voice.handoff_remaining);
            const i32 current=output[i]/65536;
            const i32 value=(voice.handoff_sample*remaining+current*(128-remaining))/128;
            output[i]=(!pitch[i]||pitch[i]>=0x7c000000u)?0:value*65536;
            if(voice.handoff_remaining)--voice.handoff_remaining;
        }
    }
    voice.last_sample=output[63]/65536;
}
}
