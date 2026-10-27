#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include "engine.cpp"
static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(){
    using namespace troupe;
    for(int x=-32767;x<=32767;++x)require(quiet_fundamental(x)==x/10,"Exact signed cold-sine division across its entire range");
    PreparedCache cache={};opxy::i16 wave[256],copy[256];
    for(unsigned id=0;id<200;++id){
        WaveKey key={id/29,id%29,0,id%5,0};
        bank_decode(raw_table(key),0,256,wave);remember_prepared(&cache,key,wave);
        require(cache.count<=64&&cache.next<64,"Bounded completed-wave storage");
        require(cached_prepared(&cache,key,copy)&&std::equal(wave,wave+256,copy),"Exact key and wave after wraparound");
        const unsigned next=cache.next;remember_prepared(&cache,key,wave);require(cache.next==next,"Duplicate completion preserves other entries");
        WaveKey neighbor=key;neighbor.pitch_mix=1;
        require(!cached_prepared(&cache,neighbor,copy),"Fractional pitch is part of the immutable key");
        neighbor=key;neighbor.vowel_mix=1;
        require(!cached_prepared(&cache,neighbor,copy),"Fractional vowel is part of the immutable key");
    }
    Shared shared={};Voice voices[24]={};int p[8]={794568949,1181116006,0,1073741824},out[64];unsigned pitch[64];
    unsigned hits=0,cold_slices=0;
    for(unsigned bar=0;bar<16;++bar){
        for(unsigned n=0;n<24;++n)start(voices[n],shared,n+1);
        for(unsigned block=0;block<96;++block)for(unsigned n=0;n<24;++n){
            p[1]=static_cast<int>((2*(n%10)+1)*2147483647LL/20);
            std::fill(pitch,pitch+64,bank_pitch[3+n%20]+123456);
            render(voices[n],shared,n+1,out,p,pitch);
            auto* state=shared.voices.get(n+1,voices[n].lease);require(state!=nullptr,"Retain all 24 sounding notes");
            if(state->building&&state->active<2){
                if(state->cached_build)++hits;
                else{require(bar==0,"Repeated bars bypass every predictive blend slice");++cold_slices;}
            }
            if(bar&&block==0)require(state->cached_build&&state->cursor==16,"Warm notes keep their original preparation timeline without decoding");
            for(int value:out)require(value>=-805306368&&value<=805306368,"Original declared output peak");
        }
    }
    require(cold_slices>0&&hits>5000,"Both cold construction and repeated cached preparations are exercised");
    std::printf("PASS: all 65535 signed cold-sine values; exact keyed wave eviction/duplicates; 16 bars x24 notes; %u cold slices and %u cached slices with preserved timing.\n",cold_slices,hits);
}
