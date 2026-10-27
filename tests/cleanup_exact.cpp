#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "engine.cpp"
#include "../display.cpp"
#include "frozen/cast_engine.h"
#include "frozen/cast_display.h"

static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static_assert(sizeof(troupe::RenderVoice)==1320,"One audible singer, two PRNG-only timers");
static_assert(sizeof(approved_cast::RenderVoice)-sizeof(troupe::RenderVoice)==80,"Remove exactly 80 obsolete bytes per note");
static_assert(sizeof(approved_cast::Shared)-sizeof(troupe::Shared)==1920,"Retain all 24 note leases and both caches");
static_assert(troupe::active_notes==24&&troupe::max_singers==1,"One audio singer per note, 24 simultaneous notes");
static unsigned long long samples=0,wave_samples=0,frames=0;
static bool key_equal(const troupe::WaveKey& a,const approved_cast::WaveKey& b){
    return a.type==b.type&&a.pitch==b.pitch&&a.pitch_mix==b.pitch_mix&&a.vowel==b.vowel&&a.vowel_mix==b.vowel_mix;
}
static void state_equal(const troupe::RenderVoice& a,const approved_cast::RenderVoice& b){
    require(a.random==b.random,"Shared PRNG stays exact after all lead/removed-companion draws");
    require(a.phase[0]==b.phase[0]&&a.gains[0]==b.gains[0]&&a.pitch_offset[0]==b.pitch_offset[0],"Lead oscillator, gain and tuning continuity");
    require(a.vibrato_phase[0]==b.vibrato_phase[0]&&a.drift_wait[0]==b.drift_wait[0]&&a.drift[0]==b.drift[0]&&a.drift_target[0]==b.drift_target[0],"Audible expression remains exact");
    require(a.companion_drift_wait[0]==b.drift_wait[1]&&a.companion_drift_wait[1]==b.drift_wait[2],"Discarded random draws keep their original timing");
    require(a.lowpass==b.lowpass&&a.effort==b.effort&&a.air_low==b.air_low&&a.air_floor==b.air_floor&&a.air_gain==b.air_gain,"Filter, Effort and breath state remains exact");
    require(a.initialized==b.initialized&&a.active==b.active&&a.building==b.building&&a.cursor==b.cursor&&a.fade==b.fade&&a.age==b.age&&a.cached_build==b.cached_build,"Preparation and crossfade timing remains exact");
    require(a.attack_phase==b.attack_phase&&a.attack_step==b.attack_step&&a.vibrato_wait==b.vibrato_wait&&a.vibrato_envelope==b.vibrato_envelope&&a.vibrato_fade_step==b.vibrato_fade_step&&a.vibrato_depth==b.vibrato_depth,"Onsets and delayed vibrato remain exact");
    require(key_equal(a.current,b.current)&&key_equal(a.target,b.target),"Prepared identities remain exact");
    require(std::memcmp(a.tables,b.tables,sizeof(a.tables))==0,"Every prepared waveform sample remains exact");
}
static void equal_block(const int* a,const int* b){
    require(std::equal(a,a+64,b),"Every output sample matches the approved Cast renderer");samples+=64;
}
static void render_pair(troupe::RenderVoice& a,approved_cast::RenderVoice& b,int* p,unsigned* pitch){
    int x[64],y[64];troupe::render_sound(a,x,p,pitch);approved_cast::render_sound(b,y,p,pitch);
    equal_block(x,y);state_equal(a,b);
}
static void anchor_checks(){
    int p[8]={};unsigned pitch[64];
    for(unsigned type=0;type<10;++type)for(unsigned anchor=0;anchor<29;++anchor)
    for(unsigned vowel=0;vowel<5;++vowel)for(unsigned cast=0;cast<3;++cast)for(unsigned effort=0;effort<2;++effort){
        troupe::RenderVoice a={};approved_cast::RenderVoice b={};
        const unsigned owner=1+(type*73+anchor*17+vowel)%256;
        troupe::begin_sound(a,owner);approved_cast::begin_sound(b,owner);
        p[0]=static_cast<int>(vowel*2147483647LL/4);p[1]=static_cast<int>((2*type+1)*2147483647LL/20);
        p[2]=static_cast<int>(cast*2147483647LL/2);p[3]=effort?INT_MAX:0;
        unsigned base=troupe::bank_pitch[anchor];
        if(anchor<28&&(vowel&1))base+=(troupe::bank_pitch[anchor+1]-base)/2;
        std::fill(pitch,pitch+64,base);
        for(unsigned block=0;block<128;++block)render_pair(a,b,p,pitch);
    }
}
static void long_and_interrupted_checks(){
    int p[8]={};unsigned pitch[64];
    for(unsigned owner=1;owner<=24;++owner){
        troupe::RenderVoice a={};approved_cast::RenderVoice b={};
        troupe::begin_sound(a,owner);approved_cast::begin_sound(b,owner);
        for(unsigned block=0;block<8192;++block){
            p[0]=static_cast<int>((block*173u%32768u)*65536u);
            p[1]=static_cast<int>((2*((owner+block/333)%10)+1)*2147483647LL/20);
            // Full Cast, near-full (255/256), intermediate, zero, and rapid
            // retargeting exercise state left untouched by the fast path.
            if(block%1024<256)p[2]=INT_MAX;
            else if(block%1024<512)p[2]=2139095040;
            else if(block%1024<768)p[2]=0;
            else p[2]=static_cast<int>((block*127u%32768u)*65536u);
            p[3]=block%512<256?INT_MAX:0;
            for(unsigned i=0;i<64;++i){
                const unsigned j=(owner+block/71)%29;
                pitch[i]=troupe::bank_pitch[j]+(i*137u);
                if(block%97==0&&i%3==0)pitch[i]=0;
                if(block%101==0&&i%7==0)pitch[i]=0x7c000000u;
                if(block%103==0&&i%5==0)pitch[i]=510000000u;
            }
            if(block%2048==2047){troupe::begin_sound(a,owner);approved_cast::begin_sound(b,owner);}
            render_pair(a,b,p,pitch);
        }
    }
}
static void shared_checks(){
    static troupe::Shared a={};static approved_cast::Shared b={};
    troupe::Voice av[256]={};approved_cast::Voice bv[256]={};int p[8]={},x[64],y[64];unsigned pitch[64];
    for(unsigned id=0;id<256;++id){troupe::initialize(av[id],a,id+1);approved_cast::initialize(bv[id],b,id+1);}
    for(unsigned bar=0;bar<16;++bar){
        for(unsigned n=0;n<25;++n){const unsigned id=(n*37+19)%256;troupe::start(av[id],a,id+1);approved_cast::start(bv[id],b,id+1);}
        for(unsigned block=0;block<128;++block)for(unsigned n=0;n<25;++n){
            const unsigned id=(n*37+19)%256;
            p[0]=static_cast<int>((block*997u%32768u)*65536u);p[1]=static_cast<int>((2*(n%10)+1)*2147483647LL/20);
            p[2]=block<48?INT_MAX:block<96?0:1073741824;p[3]=static_cast<int>((block*79u%32768u)*65536u);
            std::fill(pitch,pitch+64,troupe::bank_pitch[3+n%24]+12345);
            troupe::render(av[id],a,id+1,x,p,pitch);approved_cast::render(bv[id],b,id+1,y,p,pitch);equal_block(x,y);
            require(std::memcmp(&av[id],&bv[id],sizeof(av[id]))==0,"Handle continuity and stolen tails remain exact");
            const auto* aa=a.voices.get(id+1,av[id].lease);const auto* bb=b.voices.get(id+1,bv[id].lease);
            require((aa!=nullptr)==(bb!=nullptr),"Same sparse note owns each lease");if(aa)state_equal(*aa,*bb);
        }
        require(std::memcmp(&a.startup,&b.startup,sizeof(a.startup))==0,"Startup cache contents and replacement stay exact");
        require(std::memcmp(&a.prepared,&b.prepared,sizeof(a.prepared))==0,"Prepared cache contents and replacement stay exact");
        for(unsigned n=0;n<25;++n){const unsigned id=(n*37+19)%256;troupe::destroy(av[id],a,id+1);approved_cast::destroy(bv[id],b,id+1);}
    }
}
static void slice_checks(){
    const unsigned amounts[]={0,1,127,128,255,256};
    for(unsigned table=0;table<1450;++table)for(unsigned amount:amounts)for(unsigned fraction=0;fraction<2;++fraction){
        troupe::WaveKey key={table/145|(amount<<8),(table%145)/5,0,table%5,0};
        if(fraction){key.pitch_mix=key.pitch<28?173:0;key.vowel_mix=key.vowel<4?24577:0;}
        approved_cast::WaveKey old={key.type,key.pitch,key.pitch_mix,key.vowel,key.vowel_mix};
        troupe::BankStream streams[4]={};troupe::BankStream untouched[4]={};troupe::BankStream prior[4]={};
        short actual[256]={},expected[256]={};
        for(unsigned at=0;at<256;at+=16){
            troupe::bank_slice(key,at,actual+at,streams);approved_cast::bank_slice(old,at,expected+at,prior);
            if(amount==256)require(std::memcmp(streams,untouched,sizeof(streams))==0,"Full Cast performs no lead predictive decoding");
        }
        require(std::equal(actual,actual+256,expected),"All full/intermediate Cast slices match the approved interpolation and blend order");
        wave_samples+=256;
    }
}
static void visual_checks(){
    troupe::DisplayState a={},b={};troupe::Frame x={},y={};opxy::draw::Input input={};
    troupe::display_reset(a);approved_cast::display_reset(b);
    for(unsigned tick=0;tick<8192;++tick){
        input.elapsed=tick%37==0?102:17;input.track=(tick/512)%8;
        input.slots[0]=static_cast<int>((tick*137u%32768u)*65536u);
        input.slots[1]=static_cast<int>((2*((tick/7)%10)+1)*2147483647LL/20);
        input.slots[2]=tick%3==0?INT_MAX:static_cast<int>((tick*313u%32768u)*65536u);
        input.slots[3]=static_cast<int>((tick*177u%32768u)*65536u);
        input.held_notes=tick%23<18?1+tick%24:0;input.note_serial=tick;input.note_on_serial=tick/3;
        input.reduced_motion=tick%113<17;
        if(tick%97==0){troupe::display_reset(a);approved_cast::display_reset(b);}
        troupe::display(a,input,x);approved_cast::display(b,input,y);
        require(std::memcmp(&x,&y,sizeof(x))==0&&std::memcmp(&a,&b,sizeof(a))==0,"All Cast display states and composed frames remain byte-identical");++frames;
    }
}
int main(){
    slice_checks();anchor_checks();long_and_interrupted_checks();shared_checks();visual_checks();
    std::printf("PASS: %llu exact approved-Cast audio samples; %llu exact prepared samples; %llu byte-identical visual frames; PRNG/filter/control continuity; 24-note pool, 25th-note stealing, warm/cold caches and full-Cast decoder bypass.\n",samples,wave_samples,frames);
}
