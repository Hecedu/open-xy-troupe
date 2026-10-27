#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <algorithm>
#include "engine.cpp"
static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static unsigned increment(int note){return static_cast<unsigned>(440*std::pow(2.0,(note-69)/12.0)*4294967296.0/44100);}
int main(){
    using namespace troupe;
    Shared shared={};Voice voices[256]={};int out[64],params[8]={1073741824,1252698794,0,1073741824};unsigned pitch[64];
    for(unsigned n=0;n<256;++n)initialize(voices[n],shared,n+1);
    unsigned ids[24];for(unsigned n=0;n<24;++n){ids[n]=(n*37+19)%256;start(voices[ids[n]],shared,ids[n]+1);}
    double energy[24]={};unsigned maximum=0;unsigned built=0;
    for(unsigned block=0;block<4096;++block){
        for(unsigned n=0;n<24;++n){
            const unsigned owner=ids[n]+1;RenderVoice* state=shared.voices.get(owner,voices[ids[n]].lease);
            require(state!=nullptr,"All 24 sparse global handles retain their own state");
            for(unsigned i=0;i<64;++i)pitch[i]=increment(24+static_cast<int>(n)*3)+(block%5)*1000+i*30;
            if(block%64==0){
                params[0]=static_cast<int>((block/64%5)*536870911u);
                params[1]=static_cast<int>(((block/64%10)*2+1)*107374182u);
                params[2]=(block/64)%2?2147483647:0;
                params[3]=static_cast<int>((block/64%3)*1073741823u);
            }
            const unsigned before=state->cursor;
            const bool building=state->building;const bool cold=state->active==2;
            render(voices[ids[n]],shared,owner,out,params,pitch);
            require(state->cursor<=256,"Bounded table cursor");
            if(state->cursor&&state->cursor!=before){
                require((building&&state->cursor==before+(cold?64:16))||(!building&&state->cursor==(cold?64:16)),"At most 64 cold or 16 morph samples per callback");++built;
            }
            for(int value:out){require(value>=-805306368&&value<=805306368,"Declared output headroom at all corners");maximum=std::max(maximum,static_cast<unsigned>(std::abs(value)));energy[n]+=static_cast<double>(value)*value;}
        }
    }
    for(double value:energy)require(value>1e18,"All 24 notes sound through control changes");
    require(built>20000,"Sustained simultaneous table morph workload exercised");
    const unsigned stolen=ids[0],extra=3;
    start(voices[extra],shared,extra+1);
    require(!shared.voices.get(stolen+1,voices[stolen].lease),"25th note steals oldest lease safely");
    for(unsigned b=0;b<2;++b)render(voices[stolen],shared,stolen+1,out,params,pitch);
    render(voices[stolen],shared,stolen+1,out,params,pitch);for(int v:out)require(v==0,"Stolen voice is silent after the bounded handoff");
    destroy(voices[stolen],shared,stolen+1);require(shared.voices.get(extra+1,voices[extra].lease)!=nullptr,"Late destroy cannot release stolen state");
    for(unsigned n=0;n<256;++n)destroy(voices[n],shared,n+1);
    for(unsigned n=0;n<24;++n){render(voices[ids[n]],shared,ids[n]+1,out,params,pitch);for(int v:out)require(v==0,"Released note is silent");}
    // Morph endpoints, upper note fallback, invalid/extreme host controls,
    // repeated starts and ownership churn under the integer/ASan sanitizers.
    for(unsigned pass=0;pass<10000;++pass){
        const unsigned n=(pass*47)%256;start(voices[n],shared,n+1);
        params[0]=pass%2?-2147483647:2147483647;params[1]=params[0];params[2]=params[0];params[3]=params[0];
        for(unsigned i=0;i<64;++i)pitch[i]=pass%4==0?0:pass%4==1?0xffffffff:pass%4==2?increment(127):increment(0);
        render(voices[n],shared,n+1,out,params,pitch);
        for(int v:out){require(v>=-805306368&&v<=805306368,"Edge input headroom");if(pass%4==0||pass%4==1)require(v==0,"Zero and above-Nyquist pitches are silent");}
    }
    std::printf("PASS: 24 active sparse notes, control/pitch changes, %u table-build blocks, 25th-note stealing, late release, 10,000 edge/restrike cases. Shared %zu bytes; state %zu; peak %u.\n",built,sizeof(Shared),sizeof(RenderVoice),maximum);
}
