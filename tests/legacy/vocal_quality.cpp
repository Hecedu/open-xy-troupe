#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include "engine.cpp"
static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static double band_power(const double* samples,unsigned count,double low,double high){
    double power=0;
    for(unsigned bin=0;bin<32;++bin){
        const double frequency=low+(high-low)*(bin+.5)/32;
        const double coefficient=2*std::cos(6.283185307179586*frequency/44100);
        double first=0,second=0;
        for(unsigned i=0;i<count;++i){const double next=samples[i]+coefficient*first-second;second=first;first=next;}
        power+=first*first+second*second-coefficient*first*second;
    }
    return power/32;
}
int main(){
    using namespace recital;
    int p[8]={1073741824,1252698794,0,1073741824},out[64];unsigned pitch[64];
    std::fill(pitch,pitch+64,42852281u);
    require(air_envelope(0)==0&&air_envelope(32768)==32768,"Breath is silent at start and settles at sustain");
    require(air_envelope(8192)>attack_curve(8192)*6,"Aspiration leads phonation");
    for(unsigned phase=0;phase<=32768;++phase)require(air_envelope(phase)>=0&&air_envelope(phase)<=49152,"Bounded breath envelope");
    for(unsigned owner=1;owner<=24;++owner){
        RenderVoice note={};begin_sound(note,owner);p[2]=0;
        for(unsigned block=0;block<256;++block)render_sound(note,out,p,pitch);
        require(note.gains[0]==32768&&note.gains[1]==0&&note.gains[2]==0,"True solo remains isolated");
        const unsigned attack=note.attack_phase;unsigned first[2]={1000,1000};
        p[2]=2147483647;
        for(unsigned block=0;block<256;++block){
            render_sound(note,out,p,pitch);
            require(note.attack_phase==attack,"Ensemble wake does not retrigger solo attack");
            require(note.gains[0]+note.gains[1]+note.gains[2]>=32766&&note.gains[0]+note.gains[1]+note.gains[2]<=32768,"Waiting singers retain normalized headroom");
            for(unsigned s=1;s<3;++s)if(note.gains[s]&&first[s-1]==1000)first[s-1]=block;
            if(block<3)require(note.gains[0]==32768,"Waiting companions do not duck solo");
            for(int value:out)require(value>=-805306368&&value<=805306368,"Wake headroom");
            require(note.air_low>=-8192&&note.air_low<=8192&&note.air_floor>=-8192&&note.air_floor<=8192,"Aspiration filter state is bounded");
        }
        require(first[0]>=4&&first[0]<first[1]&&first[1]<25,"Companions enter separately within bounded delays");
        for(unsigned block=0;block<128;++block){
            p[2]=block%2?0:2147483647;
            const unsigned a=note.entry_phase[0],b=note.entry_phase[1];
            render_sound(note,out,p,pitch);
            require(note.entry_phase[0]>=a&&note.entry_phase[1]>=b,"Rapid interrupted fades retain audible entrance progress");
        }
        p[2]=0;
        for(unsigned block=0;block<128;++block)render_sound(note,out,p,pitch);
        require(note.entry_mask==0&&note.gains[0]==32768&&note.gains[1]==0&&note.gains[2]==0,"Returning to solo fully rearms companions");
        p[2]=2147483647;render_sound(note,out,p,pitch);
        require(note.gains[0]==32768&&note.gains[1]==0&&note.gains[2]==0,"Rearmed companions get a fresh soft entrance");
    }
    // Actual noise-only output: silent periodic tables, a settled note and
    // unchanged valid pitch/control key isolate aspiration from phonation.
    RenderVoice noise={};begin_sound(noise,17);p[2]=p[3]=0;
    noise.current=key_for(p,pitch[0]);noise.initialized=1;noise.active=0;
    noise.attack_phase=32768;noise.air_gain=character_air[noise.current.type];
    long long energy=0;double samples[8192];
    for(unsigned block=0;block<256;++block){
        render_sound(noise,out,p,pitch);
        for(unsigned i=0;i<64;++i){const int value=out[i]/65536;energy+=static_cast<long long>(value)*value;if(block>=128)samples[(block-128)*64+i]=value;}
    }
    require(energy>10000,"Soft sustained singing has audible filtered aspiration");
    const double low=band_power(samples,8192,40,250),middle=band_power(samples,8192,1500,5000),high=band_power(samples,8192,15000,21000);
    require(middle>low*2&&middle>high*2,"Rendered aspiration is band-shaped rather than broadband hiss or low rumble");
    p[3]=2147483647;
    for(unsigned block=0;block<256;++block)render_sound(noise,out,p,pitch);
    for(int value:out)require(value==0,"Firm effort suppresses aspiration without a stuck noise floor");
    std::printf("PASS: leading/settled breath, noise isolation, 24 independent staggered entrances, solo continuity, normalization, interrupted fades and rearming. Shared %zu bytes.\n",sizeof(Shared));
}
