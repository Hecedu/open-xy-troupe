// Frozen recital-decoder-001 renderer (2304b7e), retained for original fingerprints
// and independent companion-sampling comparison. Production uses ../engine.cpp.
#include "core.h"
#include "bank.h"
#include "effort.h"
#include "expression.h"
namespace recital {
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
    WaveKey key={voice_type(parameters[1]),0,0,0,0};
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
inline unsigned raw_table(const WaveKey& key){return (key.type*29+key.pitch)*5+key.vowel;}
// Decode each pitch/vowel corner once for this 16-sample destination slice.
// Exact i32 blend order matches the original per-sample bank interpolation.
inline void bank_slice(const WaveKey& key,unsigned start,opxy::i16* output,BankStream* streams){
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
inline void begin_sound(RenderVoice& state,u32 owner){
    state.random=0x91e10da5u^(owner*0x9e3779b9u);if(!state.random)state.random=1;
    for(unsigned s=0;s<3;++s){state.phase[s]=random_word(state.random);state.gains[s]=0;}
    state.initialized=state.building=state.cursor=state.fade=state.age=state.cached_build=0;
    state.active=2;state.lowpass=state.effort=state.ensemble=0;
    state.attack_phase=state.attack_step=state.vibrato_wait=state.vibrato_envelope=state.vibrato_fade_step=0;
    state.vibrato_depth=0;
    state.air_low=state.air_floor=state.air_gain=0;
    state.entry_mask=0;
    for(unsigned s=0;s<2;++s){state.entry_phase[s]=state.entry_wait[s]=state.entry_step[s]=0;}
    for(unsigned s=0;s<3;++s){
        state.vibrato_phase[s]=random_word(state.random);
        state.drift_wait[s]=128+(random_word(state.random)&255);
        state.drift[s]=state.drift_target[s]=state.pitch_offset[s]=0;
    }
}
inline i32 sample(const opxy::i16* table,u32 phase){
    const u32 index=phase>>24;
    const i32 a=table[index],b=table[(index+1)&255];
    return a+(b-a)*static_cast<i32>((phase>>16)&255)/256;
}
// Identical steady-table oscillator with block choices specialized once.
template<bool First, bool Unity>
inline void steady_oscillator(i32* mixed, const opxy::i16* table, u32& phase,
                              i32 gain, i32 gain_step, i32 offset,
                              i32 offset_step, opxy::Pitch pitch) {
    for (unsigned i=0; i<64; ++i) {
        gain += gain_step;
        const i32 value = sample(table, phase);
        const i32 contribution = Unity ? value : value*gain/32768;
        offset += offset_step;
        phase += pitch[i] + static_cast<u32>(offset);
        if (First) mixed[i] = pitch[i]>bank_pitch[28] ? sample(sine, phase)/4 : contribution;
        else if (pitch[i]<=bank_pitch[28]) mixed[i] += contribution;
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
// A cold cache miss resumes only 64 source samples per callback. A quiet
// fundamental carries the existing onset until that wave can fade in. Warm
// notes copy one cached wave; subsequent morphs retain their 16-sample budget.
inline void update_table(RenderVoice& state,const WaveKey& wanted,StartupCache* cache=nullptr,PreparedCache* prepared=nullptr){
    if(!state.initialized){
        state.current=wanted;state.current.pitch_mix=state.current.vowel_mix=0;
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
// Only two companions, updated once per block. Their effective weights join
// the existing normalization, so a waiting singer cannot duck the soloist.
inline void entrance_weights(RenderVoice& state,i32* weights,const CharacterExpression& character,u32 owner_seed){
    for(unsigned s=1;s<3;++s){
        const unsigned j=s-1,bit=1u<<j;
        if(!weights[s]&&!state.gains[s]){
            state.entry_mask&=~bit;state.entry_phase[j]=0;continue;
        }
        if(!(state.entry_mask&bit)){
            state.entry_mask|=bit;state.entry_phase[j]=0;
            // 6-10 ms and 15-19 ms; independent of the audio PRNG sequence.
            state.entry_wait[j]=(s==1?4u:10u)+((owner_seed>>(j*3))&3u);
            state.entry_step[j]=character.attack_step;
        }
        if(state.entry_wait[j])--state.entry_wait[j];
        else state.entry_phase[j]=static_cast<u32>(bound(static_cast<i32>(state.entry_phase[j]+state.entry_step[j]),0,32768));
        weights[s]=weights[s]*attack_curve(state.entry_phase[j])/32768;
    }
}
inline i32 air_envelope(u32 phase){
    const i32 lead=attack_curve(phase<8192?phase*4:32768);
    return lead*(49152-attack_curve(phase)/2)/32768;
}
// Products stay within signed 32 bits even for the full unsigned pitch input.
inline i32 pitch_delta(u32 pitch,i32 tuning){
    return static_cast<i32>(pitch>>16)*tuning+static_cast<i32>(pitch&65535)*tuning/65536;
}
// A fixed three-singer control update. The audio loop only ramps increments;
// no audio-rate LFO, random drift generation or software-wide arithmetic.
inline void update_expression(RenderVoice& state,const CharacterExpression& character,i32* targets){
    if(state.vibrato_wait)--state.vibrato_wait;
    else state.vibrato_envelope=static_cast<u32>(bound(static_cast<i32>(state.vibrato_envelope+state.vibrato_fade_step),0,32768));
    state.vibrato_depth+=(character.vibrato_depth-state.vibrato_depth)/8;
    const i32 depth=state.vibrato_depth*static_cast<i32>(state.vibrato_envelope)/32768;
    const u32 rates[3]={character.vibrato_rate,character.vibrato_rate+(character.vibrato_rate>>5),character.vibrato_rate-(character.vibrato_rate>>6)};
    for(unsigned s=0;s<3;++s){
        if(state.drift_wait[s])--state.drift_wait[s];
        else{
            state.drift_target[s]=(static_cast<i32>(random_word(state.random)&127)-64)*256;
            state.drift_wait[s]=256+(random_word(state.random)&255);
        }
        state.drift[s]+=(state.drift_target[s]-state.drift[s])/128;
        const i32 vibrato=depth?sample(sine,state.vibrato_phase[s])*depth/32768:0;
        targets[s]=(s==0?0:s==1?-600:600)+vibrato+state.drift[s]/256;
        state.vibrato_phase[s]+=rates[s];
    }
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
    const CharacterExpression& character=character_expression[wanted.type];
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
    const i32 previous_ensemble=state.ensemble,previous_effort=state.effort;
    const i32 target_ensemble=effort_value(parameters[2]);
    state.ensemble+=(target_ensemble-state.ensemble)/4;
    if(target_ensemble==0&&state.ensemble<4)state.ensemble=0;
    i32 weight[3]={32768,bound(state.ensemble*2,0,32768),bound(state.ensemble*2-32768,0,32768)};
    entrance_weights(state,weight,character,state.phase[0] ^ state.vibrato_phase[0]);
    const i32 total=weight[0]+weight[1]+weight[2];
    i32 goal[3],step[3],gain[3];
    for(unsigned s=0;s<3;++s){
        goal[s]=total==32768?weight[s]:weight[s]?weight[s]*32768/total:0;
        gain[s]=state.gains[s];step[s]=(goal[s]-gain[s])/64;
    }
    state.effort+=(effort_value(parameters[3])-state.effort)/8;
    // Keep the authored Effort response while preventing its fixed low-pass
    // corner from swallowing upper-register fundamentals. Unity is stable.
    const i32 brightness=bound(10000+state.effort*22000/32768,
                               static_cast<i32>(pitch[0]>>12)>32768?32768:static_cast<i32>(pitch[0]>>12),32768);
    if(fresh)state.air_gain=character_air[wanted.type];
    else state.air_gain+=(character_air[wanted.type]-state.air_gain)/8;
    const i32 air=(32768-state.effort)/512*state.air_gain/256;
    // +3 dB in solo, gradually reaching +5.35 dB with both companions.
    // Retain the existing normalized singer weights and declared .375 peak.
    const i32 level=46341+state.ensemble*14279/32768;
    const i32 volume_goal=(26000+state.effort/4)*level/32768;
    const i32 volume_start=(26000+previous_effort/4)*(46341+previous_ensemble*14279/32768)/32768;
    const unsigned singers=(gain[2]||goal[2])?3:(gain[1]||goal[1])?2:1;
    i32 tuning[3],offsets[3],offset_steps[3],offset_goals[3];
    update_expression(state,character,tuning);
    for(unsigned s=0;s<3;++s){
        offset_goals[s]=pitch_delta(pitch[0],tuning[s]);
        offsets[s]=fresh?offset_goals[s]:state.pitch_offset[s];
        offset_steps[s]=(offset_goals[s]-offsets[s])/64;
    }
    const u32 next_attack=static_cast<u32>(bound(static_cast<i32>(state.attack_phase+state.attack_step),0,32768));
    i32 onset=attack_curve(state.attack_phase);
    const i32 onset_step=(attack_curve(next_attack)-onset)/64;
    i32 breath_onset=air_envelope(state.attack_phase);
    const i32 breath_step=(air_envelope(next_attack)-breath_onset)/64;
    // Each singer's sample stream is independent within this callback.
    // Complete one stream at a time so phase, gain and tuning stay local;
    // accumulate in the same singer order for every destination sample.
    i32 mixed[64];
    for(unsigned s=0;s<singers;++s){
        u32 phase=state.phase[s];
        i32 singer_gain=gain[s],offset=offsets[s];
        const i32 gain_step=step[s],offset_step=offset_steps[s];
        const bool unity=singer_gain==32768&&gain_step==0;
        if(from && !fading) {
            if(s==0) {
                if(unity) steady_oscillator<true,true>(mixed,from,phase,singer_gain,gain_step,offset,offset_step,pitch);
                else steady_oscillator<true,false>(mixed,from,phase,singer_gain,gain_step,offset,offset_step,pitch);
            } else {
                if(unity) steady_oscillator<false,true>(mixed,from,phase,singer_gain,gain_step,offset,offset_step,pitch);
                else steady_oscillator<false,false>(mixed,from,phase,singer_gain,gain_step,offset,offset_step,pitch);
            }
        } else for(unsigned i=0;i<64;++i){
            singer_gain+=gain_step;
            i32 value=from?sample(from,phase):quiet_fundamental(sample(sine,phase));
            if(fading)value=blend15(value,sample(to,phase),static_cast<i32>(state.fade+i)*256);
            const i32 contribution=unity?value:value*singer_gain/32768;
            offset+=offset_step;
            phase+=pitch[i]+static_cast<u32>(offset);
            // The original upper fallback uses the primary phase after this
            // sample's increment and overrides the other singers' sum.
            if(s==0)mixed[i]=pitch[i]>bank_pitch[28]?sample(sine,phase)/4:contribution;
            else if(pitch[i]<=bank_pitch[28])mixed[i]+=contribution;
        }
        state.phase[s]=phase;
    }
    if(state.attack_phase==32768&&volume_start==volume_goal)
        output_pass<true>(state,output,pitch,mixed,brightness,air,volume_start,volume_goal,onset,onset_step,breath_onset,breath_step);
    else output_pass<false>(state,output,pitch,mixed,brightness,air,volume_start,volume_goal,onset,onset_step,breath_onset,breath_step);
    state.attack_phase=next_attack;
    if(state.age<0x7fffffbfu)state.age+=64;
    for(unsigned s=0;s<3;++s)state.pitch_offset[s]=offset_goals[s];
    for(unsigned s=0;s<3;++s)state.gains[s]=goal[s];
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
