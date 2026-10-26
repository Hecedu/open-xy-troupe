#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include "engine.cpp"
static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static_assert(recital::active_notes==24,"Retain the 24-note renderer");
static_assert(sizeof(recital::BankStream)==36&&sizeof(recital::Shared)==84368,"Four bounded resume states per note plus fixed startup and prepared caches");
int main(){
    using namespace recital;
    int previous=-1;
    for(int word=0;word<=32768;++word){
        const int value=vowel_curve(word);
        require(value>=0&&value<=32768&&value>=previous,"Bounded monotonic vowel mapping without reversals");
        previous=value;
    }
    require(vowel_curve(0)==0&&vowel_curve(16384)==16384&&vowel_curve(32768)==32768,"Exact endpoints and segment midpoint");
    int parameters[8]={};
    for(unsigned v=0;v<=4;++v){
        parameters[0]=v==4?2147483647:static_cast<int>(v*536870912u);
        const auto key=key_for(parameters,bank_pitch[12]);
        require(key.vowel==v&&key.vowel_mix==0,"Five canonical vowels keep exact authored waves");
    }
    Shared shared={};Voice voices[256]={};int output[64];unsigned pitch[64];
    for(unsigned n=0;n<256;++n)initialize(voices[n],shared,n+1);
    const unsigned counts[]={1,2,4,8,12,24};unsigned started=0,build_blocks=0;
    for(unsigned count:counts){
        for(;started<count;++started){const unsigned id=(started*37+19)%256;start(voices[id],shared,id+1);}
        for(unsigned b=0;b<256;++b)for(unsigned n=0;n<count;++n){
            const unsigned id=(n*37+19)%256;
            parameters[0]=static_cast<int>((b*2147483647LL)/255);
            parameters[1]=1252698794;parameters[2]=b<128?static_cast<int>(b*2147483647LL/127):static_cast<int>((255-b)*2147483647LL/127);parameters[3]=1073741824;
            std::fill(pitch,pitch+64,bank_pitch[8+n%12]);
            auto* state=shared.voices.get(id+1,voices[id].lease);require(state!=nullptr,"All held note leases survive Ensemble ramps");
            const unsigned before=state->cursor;const bool building=state->building;const bool cold=state->active==2;
            render(voices[id],shared,id+1,output,parameters,pitch);
            if(state->cursor&&state->cursor!=before){require((building&&state->cursor==before+(cold?64:16))||(!building&&state->cursor==(cold?64:16)),"Bounded 64-sample cold / 16-sample morph budget");++build_blocks;}
            for(int sample:output)require(sample>=-805306368&&sample<=805306368,"Normalized output through Ensemble rise and fall");
        }
    }
    require(build_blocks>5000,"Sustained multi-note vowel building exercised");
    std::puts("PASS: monotonic bounded vowel curve, exact authored anchors, 1/2/4/8/12/24-note Ensemble ramps, fixed 16-sample morph budget and headroom.");
}
