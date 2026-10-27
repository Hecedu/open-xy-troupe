
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <algorithm>
#include "frozen/approved_decoder_engine.h"
#include "frozen/approved_display.h"

static void require(bool ok,const char* message) {
    if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}

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

int main() {
    Digest digest;int parameters[8]={},b[64];unsigned pitch[64];unsigned long samples=0,frames=0;
    // Every authored pitch anchor and its midpoint, each character/vowel and
    // solo/full ensemble, including raw startup and the 16-sample morph build.
    for(unsigned type=0;type<10;++type)for(unsigned anchor=0;anchor<29;++anchor)
    for(unsigned middle=0;middle<2;++middle)for(unsigned vowel=0;vowel<5;++vowel)
    for(unsigned ensemble=0;ensemble<2;++ensemble) {
        recital::RenderVoice packed={};
        recital::begin_sound(packed,37);
        parameters[0]=static_cast<int>(vowel*2147483647LL/4);
        parameters[1]=static_cast<int>((2*type+1)*2147483647LL/20);
        parameters[2]=ensemble?INT_MAX:0;parameters[3]=ensemble?INT_MAX:0;
        const unsigned lo=recital::bank_pitch[anchor],hi=recital::bank_pitch[anchor<28?anchor+1:anchor];
        std::fill(pitch,pitch+64,middle?lo+(hi-lo)/2:lo);
        for(unsigned block=0;block<128;++block) {
            recital::render_sound(packed,b,parameters,pitch);
            digest.add(b,sizeof(b));digest.add(&packed,sizeof(packed));samples+=64;
        }
    }
    // A live 24-note chord: interrupted morphs, character/effort/Ensemble
    // sweeps, per-sample pitch changes, independent vibrato/drift, restrikes.
    recital::RenderVoice packed[24]={};
    for(unsigned n=0;n<24;++n){recital::begin_sound(packed[n],19+n*7);}
    for(unsigned block=0;block<3072;++block)for(unsigned note=0;note<24;++note) {
        parameters[0]=static_cast<int>((block*7919u)%32768)*65536;
        parameters[1]=static_cast<int>(((block/37)%10*2+1)*2147483647LL/20);
        parameters[2]=block%200<100?static_cast<int>((block%100)*2147483647LL/99):0;
        parameters[3]=static_cast<int>((block*3571u)%32768)*65536;
        for(unsigned i=0;i<64;++i)pitch[i]=recital::bank_pitch[2+note%25]+i*137+block%19;
        if(block%251==0){recital::begin_sound(packed[note],19+note*7);}
        recital::render_sound(packed[note],b,parameters,pitch);
        digest.add(b,sizeof(b));digest.add(&packed[note],sizeof(packed[note]));samples+=64;
    }
    recital::DisplayState state={};
    recital::Frame frame={};opxy::draw::Input input={};
    recital::display_reset(state);
    for(unsigned step=0;step<8192;++step) {
        input.slots[0]=static_cast<int>((step*3571u)%32768)*65536;
        input.slots[1]=static_cast<int>(((step/13)%10*2+1)*2147483647LL/20);
        input.slots[2]=static_cast<int>((step*7919u)%32768)*65536;
        input.slots[3]=static_cast<int>((step*73u)%32768)*65536;
        input.elapsed=step%29;input.track=(step/113)%9;input.reduced_motion=(step/71)%2;
        input.held_notes=step%31<20?3:0;input.note_serial=step/3;input.note_off_serial=step/7;
        // Compare the unchanged normal display after the intentionally new entrance.
        state.startup_initialized=1;state.startup_track=input.track;state.startup_age=recital::startup_end;
        recital::display(state,input,frame);
        digest.add(&frame,sizeof(frame));digest.add(&state,sizeof(state));++frames;
    }
    std::printf("render samples=%lu frames=%lu hash=%016llx\n",samples,frames,digest.value);
    require(digest.value==0x42b084168b78452bULL,"Frozen approved audio, state and display fingerprint");
}
