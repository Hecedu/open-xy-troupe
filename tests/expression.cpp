#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include "engine.cpp"
static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static double energy(const int* x){double e=0;for(unsigned i=0;i<64;++i)e+=static_cast<double>(x[i])*x[i];return std::sqrt(e/64);}
static void controls(int* p,unsigned type){
    const unsigned selectors[10]={1,4,5,8,9,0,6,7,2,3};
    p[0]=1073741824;p[1]=static_cast<int>((selectors[type]*2+1)*2147483647.0/20);p[2]=0;p[3]=2147483647;
}
static double measured_cents(unsigned before,unsigned after,unsigned pitch){
    const int correction=static_cast<int>(static_cast<unsigned>(after-before-pitch*64u));
    return 1200*std::log2(1+correction/(static_cast<double>(pitch)*64));
}
int main(){
    int p[8]={},out[64],reference[64];unsigned pitch[64];unsigned attack90[10]={},drift_hashes[24]={};
    std::fill(pitch,pitch+64,510000000u); // Fundamental-only range isolates attacks.
    for(unsigned type=0;type<10;++type){
        recital::RenderVoice note={},full={};recital::begin_sound(note,1);recital::begin_sound(full,1);
        full.attack_phase=32768;controls(p,type);
        bool crossed=false;
        for(unsigned block=0;block<100;++block){
            recital::render_sound(note,out,p,pitch);recital::render_sound(full,reference,p,pitch);
            const double ratio=energy(out)/energy(reference);
            require(ratio>=0&&ratio<1.001,"Attack gain remains bounded");
            if(block<2)require(ratio<.1,"Soft, click-free note entrance");
            if(!crossed&&ratio>.9){attack90[type]=block;crossed=true;}
        }
        require(crossed,"Every character reaches its full voice");
        // Changing character on a held note must not restart the entrance.
        for(unsigned next=0;next<10;++next){controls(p,next);recital::render_sound(note,out,p,pitch);require(note.attack_phase==32768&&energy(out)>1e6,"Held character selection retains the completed onset");}
    }
    require(attack90[5]<attack90[0]&&attack90[0]<attack90[1]&&attack90[1]<attack90[3]&&attack90[3]<attack90[2]&&attack90[2]<attack90[4],"Audible character-specific attack ordering");
    std::fill(pitch,pitch+64,42852281u); // A4.
    const double depth_cents[10]={1.06,7.4,4.23,18.5,22.5,3.17,10.04,2.64,4.75,.66};
    const unsigned minimum_delay[10]={550,294,343,102,136,481,378,448,308,598};
    for(unsigned type=0;type<10;++type){
        recital::RenderVoice note={};recital::begin_sound(note,1);controls(p,type);
        double maximum=0,previous=0;unsigned crossings=0;
        for(unsigned block=0;block<2048;++block){
            // Suppress drift in this fixture to measure vibrato independently.
            note.drift_wait[0]=100000;
            const unsigned before=note.phase[0];recital::render_sound(note,out,p,pitch);
            const double cents=measured_cents(before,note.phase[0],pitch[0]);
            if(block<minimum_delay[type])require(std::abs(cents)<.0001,"Vibrato begins after the character's delay");
            maximum=std::max(maximum,std::abs(cents));
            if(block>1024&&previous<=0&&cents>0)++crossings;
            previous=cents;
        }
        require(maximum>depth_cents[type]*.8&&maximum<depth_cents[type]*1.05,"Measured vibrato has its authored character depth");
        require(crossings>=6&&crossings<=9,"Measured vibrato rate remains vocal and bounded");
    }
    for(unsigned owner=1;owner<=24;++owner){
        recital::RenderVoice note={},repeat={};recital::begin_sound(note,owner);recital::begin_sound(repeat,owner);controls(p,2);
        double previous=0,minimum=0,maximum=0;unsigned fingerprint=2166136261u;
        for(unsigned block=0;block<4096;++block){
            if(block){note.vibrato_wait=repeat.vibrato_wait=100000;}
            const unsigned before=note.phase[0];recital::render_sound(note,out,p,pitch);recital::render_sound(repeat,reference,p,pitch);
            require(std::equal(out,out+64,reference),"Repeated owner/controls render deterministically");
            const double cents=measured_cents(before,note.phase[0],pitch[0]);
            fingerprint=(fingerprint^static_cast<unsigned>(note.phase[0]-before-pitch[0]*64u))*16777619u;
            require(std::abs(cents)<1.8,"Gentle drift stays below 1.8 cents");
            require(std::abs(cents-previous)<.06,"Drift has no abrupt frequency jumps");
            minimum=std::min(minimum,cents);maximum=std::max(maximum,cents);previous=cents;
        }
        require(maximum-minimum>.2,"Every held note develops gentle drift");
        for(unsigned previous_owner=1;previous_owner<owner;++previous_owner)require(fingerprint!=drift_hashes[previous_owner-1],"Every owner has an independent drift trajectory");
        drift_hashes[owner-1]=fingerprint;
    }
    std::printf("PASS: audible attack ordering, held-note character changes, measured delayed vibrato/depth/rate for ten characters, and deterministic gentle drift for 24 owners. Attack90 blocks: %u %u %u %u %u %u.\n",attack90[0],attack90[1],attack90[2],attack90[3],attack90[4],attack90[5]);
}
