
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include "frozen/approved_decoder_engine.h"
static void require(bool ok){if(!ok){std::fputs("FAIL: real callbacks differ from approved 004\n",stderr);std::exit(1);}}

// Historical fingerprints validate the frozen decoder renderer. Current audio
// is compared with an independent sampling oracle in companion_sampling.py.
// Frozen regression workload from the user-approved Choir startup-002 source.
// Hash each emitted sample and state byte; expected values were measured before renaming.
struct Digest {
    unsigned long long value=14695981039346656037ULL;
    void add(const void* data,unsigned size) {
        const unsigned char* bytes=static_cast<const unsigned char*>(data);
        for(unsigned i=0;i<size;++i) value=(value^bytes[i])*1099511628211ULL;
    }
};

int main(){
    static recital::Shared b={};recital::Voice bv[256]={};Digest digest;
    int y[64],p[8]={794568949,1181116006,0,1073741824};unsigned pitch[64];unsigned long samples=0,hits=0;
    for(unsigned bar=0;bar<24;++bar){
        // Alternate sparse global handles as stock allocators reuse objects.
        unsigned ids[24];for(unsigned n=0;n<24;++n){ids[n]=(n*7+(bar%2)*97)%256;recital::start(bv[ids[n]],b,ids[n]+1);}
        for(unsigned block=0;block<128;++block)for(unsigned n=0;n<24;++n){
            const unsigned id=ids[n];
            p[1]=static_cast<int>((2*(n%10)+1)*2147483647LL/20);p[2]=(bar%3)?2147483647:0;
            p[3]=static_cast<int>((bar%3)*2147483647LL/2);
            if(bar<16)p[0]=794568949;
            else p[0]=static_cast<int>((block*7919u)%32768)*65536;
            std::fill(pitch,pitch+64,recital::bank_pitch[3+n%20]+123456);
            if(bar>19)for(unsigned i=0;i<64;++i)pitch[i]+=i*137+block%19;
            recital::render(bv[id],b,id+1,y,p,pitch);
            digest.add(y,sizeof(y));samples+=64;
            auto* state=b.voices.get(id+1,bv[id].lease);if(state&&state->cached_build)++hits;
        }
        // Retired handles must keep identical bounded eviction tails, too.
        for(unsigned id=0;id<256;++id){
            recital::render(bv[id],b,id+1,y,p,pitch);
            digest.add(y,sizeof(y));samples+=64;
        }
    }

    const unsigned edges[9]={0,1,recital::bank_pitch[28]-1,recital::bank_pitch[28],recital::bank_pitch[28]+1,510000000u,0x7bffffffu,0x7c000000u,0xffffffffu};
    for(unsigned pattern=0;pattern<9;++pattern)for(unsigned type=0;type<10;++type)for(unsigned ensemble=0;ensemble<2;++ensemble){
        const unsigned id=(type*13+pattern)%256;
        recital::start(bv[id],b,id+1);
        p[1]=static_cast<int>((2*type+1)*2147483647LL/20);p[2]=ensemble?2147483647:0;
        for(unsigned block=0;block<64;++block){
            for(unsigned i=0;i<64;++i)pitch[i]=edges[(pattern+i/7)%9];
            recital::render(bv[id],b,id+1,y,p,pitch);
            digest.add(y,sizeof(y));samples+=64;
        }
    }
    require(hits>10000);digest.add(&b,sizeof(b));
    std::printf("callbacks samples=%lu cached_visits=%lu hash=%016llx\n",samples,hits,digest.value);
    require(digest.value==0x3e8029976d5ab8bfULL);
}
