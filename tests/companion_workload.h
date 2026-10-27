// Shared test driver, compiled separately against current, approved and oracle renderers.
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
struct Digest {
    unsigned long long value=14695981039346656037ULL;
    void add(const void* data,unsigned size){const unsigned char* p=static_cast<const unsigned char*>(data);for(unsigned i=0;i<size;++i)value=(value^p[i])*1099511628211ULL;}
};
int main(){
    Digest solo,ensemble,state;unsigned long samples=0,solo_samples=0,ensemble_samples=0;
    int p[8]={},out[64];unsigned pitch[64];
    static troupe::StartupCache startup={};static troupe::PreparedCache prepared={};
    auto collect=[&](troupe::RenderVoice& voice,bool only_lead){
        troupe::render_sound(voice,out,p,pitch,&startup,&prepared);
        for(int x:out)if(x< -805306368||x>805306368){std::fputs("Peak bound\n",stderr);std::exit(1);}
        if(only_lead){solo.add(out,sizeof(out));solo.add(&voice,sizeof(voice));solo_samples+=64;}
        else{ensemble.add(out,sizeof(out));ensemble_samples+=64;}
        // Output filter state changes with companion samples. All table work,
        // gains, phases, expression, noise, attacks and fade schedules must agree.
        troupe::RenderVoice control=voice;control.lowpass=0;
        state.add(&control,sizeof(control));samples+=64;
    };
    for(unsigned type=0;type<10;++type)for(unsigned anchor=0;anchor<29;++anchor)
    for(unsigned middle=0;middle<2;++middle)for(unsigned vowel=0;vowel<5;++vowel)
    for(unsigned ens=0;ens<3;++ens){
        troupe::RenderVoice voice={};troupe::begin_sound(voice,37);
        p[0]=static_cast<int>(vowel*2147483647LL/4);p[1]=static_cast<int>((2*type+1)*2147483647LL/20);
        p[2]=ens==0?0:ens==1?1073741824:INT_MAX;p[3]=static_cast<int>((type%3)*2147483647LL/2);
        const unsigned lo=troupe::bank_pitch[anchor],hi=troupe::bank_pitch[anchor<28?anchor+1:anchor];
        std::fill(pitch,pitch+64,middle?lo+(hi-lo)/2:lo);
        for(unsigned block=0;block<40;++block)collect(voice,ens==0);
    }
    // Interrupted vowel/pitch/character changes, Ensemble entrances and exits,
    // per-sample pitch edges, restart seeds and both cold/warm preparation caches.
    troupe::RenderVoice voice={};troupe::begin_sound(voice,91);
    for(unsigned block=0;block<8192;++block){
        p[0]=static_cast<int>((block*7919u)%32768)*65536;
        p[1]=static_cast<int>(((block/37)%10*2+1)*2147483647LL/20);
        p[2]=block%200<100?static_cast<int>((block%100)*2147483647LL/99):0;
        p[3]=static_cast<int>((block*3571u)%32768)*65536;
        for(unsigned i=0;i<64;++i)pitch[i]=troupe::bank_pitch[block%29]+i*137+block%19;
        if(block%251==0)troupe::begin_sound(voice,block+1);
        collect(voice,false);
    }
    const unsigned edges[]={0,1,troupe::bank_pitch[28]-1,troupe::bank_pitch[28],troupe::bank_pitch[28]+1,510000000u,0x7bffffffu,0x7c000000u,0xffffffffu};
    for(unsigned ens=0;ens<3;++ens){
        voice=troupe::RenderVoice();troupe::begin_sound(voice,12);
        p[2]=ens==0?0:ens==1?1073741824:INT_MAX;
        for(unsigned block=0;block<256;++block){
            for(unsigned i=0;i<64;++i)pitch[i]=edges[(block+i/7)%9];
            collect(voice,ens==0);
        }
    }
    std::printf("{\"samples\":%lu,\"solo_samples\":%lu,\"ensemble_samples\":%lu,\"solo_hash\":\"%016llx\",\"ensemble_hash\":\"%016llx\",\"control_state_hash\":\"%016llx\"}\n",samples,solo_samples,ensemble_samples,solo.value,ensemble.value,state.value);
}
