#include <cstdio>
#include <cstdlib>
#include <climits>
#include "engine.cpp"

static void require(bool ok,const char* message) {
    if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
int main() {
    using namespace recital;
    const unsigned expected[10]={5,0,8,9,1,2,6,7,3,4};
    for(unsigned slot=0;slot<10;++slot) {
        const int center=static_cast<int>((2*slot+1)*2147483647LL/20);
        require(voice_type(center)==expected[slot],"Female/male pairs select the intended audio/art identity");
    }
    require(voice_type(INT_MIN)==5&&voice_type(INT_MAX)==4,"Selector endpoints are bounded");
    for(unsigned word=0;word<32768;++word)
        require(voice_type(static_cast<int>(word*65536u))==expected[word*10/32768],
                "Every Q15 selector word belongs to the requested roster");
    // A playable midrange pitch, every voice and vowel, solo and full ensemble.
    // These checks exercise real rendering rather than only metadata/tables.
    unsigned pitch[64];int output[64],parameters[8]={};
    for(unsigned i=0;i<64;++i)pitch[i]=42852281u;
    unsigned fingerprints[10]={};
    for(unsigned slot=0;slot<10;++slot) {
        unsigned fingerprint=2166136261u;
        parameters[1]=static_cast<int>((2*slot+1)*2147483647LL/20);
        for(unsigned vowel=0;vowel<5;++vowel)for(unsigned ensemble=0;ensemble<2;++ensemble) {
            RenderVoice note={};begin_sound(note,7);
            parameters[0]=static_cast<int>(vowel*2147483647LL/4);
            parameters[2]=ensemble?INT_MAX:0;parameters[3]=INT_MAX/2;
            long long energy=0;
            for(unsigned block=0;block<512;++block) {
                render_sound(note,output,parameters,pitch);
                require(note.cursor<=256,"Character changes retain the bounded morph-table builder");
                for(int value:output) {
                    require(value>=-805306368&&value<=805306368,"Every character retains declared peak headroom");
                    if(block>=256) {
                        const int sample=value/65536;energy+=static_cast<long long>(sample)*sample;
                        fingerprint=(fingerprint^static_cast<unsigned>(sample))*16777619u;
                    }
                }
            }
            require(energy>1000000,"Every voice/vowel/ensemble setting is audible");
        }
        for(unsigned previous=0;previous<slot;++previous)
            require(fingerprint!=fingerprints[previous],"All ten rendered characters have distinct sound");
        fingerprints[slot]=fingerprint;
    }
    std::puts("PASS: ten alternating female/male selectors, exhaustive Q15 boundaries, all vowels/ensembles audible, unique rendered voices and bounded output.");
}
