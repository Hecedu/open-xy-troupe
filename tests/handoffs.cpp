#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include "engine.cpp"
static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static void bounded(const int* output){for(unsigned i=0;i<64;++i)require(output[i]>=-805306368&&output[i]<=805306368,"Handoffs retain the declared output peak");}
int main(){
    using namespace troupe;
    Shared shared={};Voice voices[256]={};int output[64],expected[64];
    int p[8]={1073741824,1181116006,966367641,1073741824};unsigned pitch[64];
    int last[24]={};
    // Two 12-note groups exhaust the shared 24-state pool. Both groups keep
    // rendering through the next note-start, as overlapping stock releases do.
    for(unsigned n=0;n<24;++n)start(voices[n],shared,n+1);
    for(unsigned block=0;block<320;++block)for(unsigned n=0;n<24;++n){
        std::fill(pitch,pitch+64,bank_pitch[10+n%12]);render(voices[n],shared,n+1,output,p,pitch);last[n]=output[63];
    }
    Shared unaffected=shared;Voice before[256];std::copy(voices,voices+256,before);
    start(voices[24],shared,25);
    require(!shared.voices.get(1,voices[0].lease),"A 25th note actually exercises render-state reassignment");
    require(std::abs(last[0]/65536)>128,"Evicted voice carries audible signal at this boundary");
    std::fill(pitch,pitch+64,bank_pitch[10]);
    int previous=last[0];
    for(unsigned block=0;block<3;++block){
        render(voices[0],shared,1,output,p,pitch);bounded(output);
        if(!block)require(output[0]==last[0],"Eviction begins at the exact previous sample instead of stepping to zero");
        for(int sample:output){
            require(std::abs(sample/65536-previous/65536)<=std::abs(last[0]/65536)/128+1,"Eviction tail moves smoothly and monotonically to silence");
            require(std::abs(sample)<=std::abs(previous),"No growing or repeated eviction tail");previous=sample;
            if(block==2)require(sample==0,"Evicted handle is completely silent after 128 frames");
        }
    }
    destroy(voices[0],shared,1);
    require(shared.voices.get(25,voices[24].lease)!=nullptr,"Late destruction cannot touch the new owner's state");
    for(unsigned n=1;n<24;++n){
        std::fill(pitch,pitch+64,bank_pitch[10+n%12]);
        render(voices[n],shared,n+1,output,p,pitch);render(before[n],unaffected,n+1,expected,p,pitch);
        require(std::equal(output,output+64,expected),"Reassignment leaves every other sounding note sample-identical");
    }
    // Every character, Ensemble endpoint and low/mid/high register. A reused
    // stock handle retains every sample of its original note attack: the
    // stock envelope owns retriggers, so no old raw sample enters a new note.
    unsigned cases=0;
    for(unsigned type=0;type<10;++type)for(unsigned ensemble=0;ensemble<2;++ensemble)
    for(unsigned anchor: {10u,16u,22u}){
        Shared group={};Voice voice={};start(voice,group,37);
        p[1]=static_cast<int>((2*type+1)*2147483647LL/20);p[2]=ensemble?2147483647:0;
        std::fill(pitch,pitch+64,bank_pitch[anchor]);
        for(unsigned block=0;block<320;++block)render(voice,group,37,output,p,pitch);
        start(voice,group,37);
        RenderVoice reference={};begin_sound(reference,37);StartupCache cache=group.startup;
        for(unsigned block=0;block<64;++block){
            render(voice,group,37,output,p,pitch);render_sound(reference,expected,p,pitch,&cache);bounded(output);
            require(std::equal(output,output+64,expected),"Every original note-attack and sustained sample remains exact");
        }
        // Repeated starts retain the normal vocal onset and never inject a
        // raw previous-note value through stock's newly restarted envelope.
        start(voice,group,37);render(voice,group,37,output,p,pitch);
        start(voice,group,37);render(voice,group,37,output,p,pitch);
        require(output[0]==0,"A new note starts on its original quiet attack");
        destroy(voice,group,37);render(voice,group,37,output,p,pitch);
        for(int sample:output)require(sample==0,"Explicit destruction clears output and every pending transition");
        ++cases;
    }
    std::printf("PASS: overlapping two-group 25th-note eviction; 128-frame monotonic tail; 23 untouched voices; %u exact character/register/Ensemble note attacks and sustains; repeated starts; late destruction.\n",cases);
}
