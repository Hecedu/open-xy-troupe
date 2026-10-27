#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include "engine.cpp"
static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static unsigned increment(unsigned note){return static_cast<unsigned>(440*std::pow(2.,(static_cast<int>(note)-69)/12.)*4294967296./44100);}
int main(){
    using namespace troupe;
    static Shared shared={};Voice voices[256]={};
    int p[8]={1073741824,1181116006,2147483647,0},out[64];unsigned pitch[64];
    // Real shared-state path: three tracks of eight sparse voices restart at
    // each simulated loop. Cache contents survive every release and reacquire.
    unsigned cold_blocks=0,warm_starts=0;
    for(unsigned loop=0;loop<32;++loop){
        for(unsigned n=0;n<24;++n){const unsigned id=(n*37+19)%256;start(voices[id],shared,id+1);}
        for(unsigned block=0;block<96;++block)for(unsigned n=0;n<24;++n){
            const unsigned id=(n*37+19)%256;
            std::fill(pitch,pitch+64,bank_pitch[3+n]+123);
            const auto wanted=key_for(p,pitch[0]);
            auto* state=shared.voices.get(id+1,voices[id].lease);
            require(state!=nullptr,"Every simultaneous note keeps its lease");
            render(voices[id],shared,id+1,out,p,pitch);
            if(state->active==2){
                require(loop==0,"Repeating bars never decode their initial waves again");
                require(state->cursor==(block+1)*64&&state->cursor<=256,"Cold work is exactly 64 source samples per callback");
                ++cold_blocks;
            }
            if(loop&&block==0){require(state->active==0,"Cached wave is playable immediately");++warm_starts;}
            for(int value:out)require(value>=-805306368&&value<=805306368,"Multi-track restart output stays within the declared peak");
            if(block>=64){
                require(same_key(state->current,wanted),"Every initial wave finishes at the intended pitch/vowel");
            }
        }
        for(unsigned n=0;n<24;++n){const unsigned id=(n*37+19)%256;destroy(voices[id],shared,id+1);}
    }
    require(cold_blocks==96&&warm_starts==744,"All cold preparations and repeated starts exercised");
    // Replacement and duplicate keys: exact cached source through wraparound.
    StartupCache cache={};opxy::i16 input[256],copy[256];
    for(unsigned table=0;table<1450;table+=7){
        bank_decode(table,0,256,input);remember_wave(&cache,table,input);
        require(cached_wave(&cache,table,copy)&&std::equal(input,input+256,copy),"Cache returns every exact decoded wave through eviction");
        const unsigned next=cache.next;remember_wave(&cache,table,input);
        require(cache.next==next,"Duplicate completion does not evict a different wave");
    }
    // Every character, vowel, Ensemble endpoint and Effort endpoint retains
    // audible upper octaves after the real onset and table preparation.
    for(unsigned type=0;type<10;++type)for(unsigned vowel=0;vowel<5;++vowel)
    for(unsigned ensemble=0;ensemble<2;++ensemble)for(unsigned effort=0;effort<2;++effort){
        p[0]=static_cast<int>(vowel*2147483647LL/4);
        p[1]=static_cast<int>((2*type+1)*2147483647LL/20);
        p[2]=ensemble?2147483647:0;p[3]=effort?2147483647:0;
        double level[3]={};
        for(unsigned octave=0;octave<3;++octave){
            RenderVoice state={};begin_sound(state,19);
            std::fill(pitch,pitch+64,increment(72+octave*12));
            for(unsigned block=0;block<384;++block){
                render_sound(state,out,p,pitch);
                for(int value:out){
                    require(value>=-805306368&&value<=805306368,"All level/brightness corners retain peak headroom");
                    if(block>=128){const double x=value/2147483648.;level[octave]+=x*x;}
                }
            }
            level[octave]=std::sqrt(level[octave]/16384);
            require(level[octave]>.025,"Every upper-register vowel has useful sustained output");
        }
        require(level[1]>level[0]*.70&&level[2]>level[0]*.70,"Upper octaves do not collapse with soft Effort or full Ensemble");
    }
    std::printf("PASS: 32 bars x 24 sparse notes; %u bounded cold blocks; %u decode-free loop restarts; exact cache replacement; 600 upper-register level corners.\n",cold_blocks,warm_starts);
}
