#include <cstdio>
#include <cstdlib>
#include <climits>
#include <algorithm>
#include <cstring>
#include "engine.cpp"
#include "../display.cpp"
static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(){
    using namespace troupe;
    int p[8]={},out[64],again[64];unsigned pitch[64];
    unsigned long samples=0;bool varied_pitch=false;
    for(unsigned type=0;type<10;++type)for(unsigned vowel=0;vowel<5;++vowel)for(unsigned amount=0;amount<3;++amount){
        RenderVoice state={},repeat={};begin_sound(state,17);begin_sound(repeat,17);
        p[0]=static_cast<int>(vowel*2147483647LL/4);
        p[1]=static_cast<int>((2*type+1)*2147483647LL/20);
        p[2]=static_cast<int>(amount*2147483647LL/2);p[3]=1073741824;
        std::fill(pitch,pitch+64,bank_pitch[12]+12345);
        long long energy=0;
        for(unsigned block=0;block<1024;++block){
            render_sound(state,out,p,pitch);render_sound(repeat,again,p,pitch);
            require(std::equal(out,out+64,again),"Identical owners and controls remain deterministic");
            require(max_singers==1,"Exactly one stored audio singer");
            require(state.gains[0]==32768,"Exactly one normalized audio singer");
            for(int value:out){require(value>=-805306368&&value<=805306368,"Declared output headroom at every voice/vowel/amount");
                if(block>512){const long long v=value/65536;energy+=v*v;}++samples;}
            if(block>128)require(state.attack_phase==32768,"All expressive onsets finish within bounded time");
            if(block>700&&state.pitch_offset[0])varied_pitch=true;
        }
        require(energy>1000000,"All voice/vowel/amount combinations are audible");
        const auto complete=state.attack_phase;
        for(unsigned block=0;block<128;++block){p[2]=block%2?0:INT_MAX;render_sound(state,out,p,pitch);
            require(state.attack_phase==complete,"Live knob changes never retrigger a held onset");}
    }
    require(varied_pitch,"Expressive delayed pitch movement is exercised");
    static Shared shared={};Voice voices[24]={};
    for(unsigned n=0;n<24;++n)start(voices[n],shared,n+1);
    for(unsigned block=0;block<256;++block)for(unsigned n=0;n<24;++n){
        p[0]=static_cast<int>((block*7919u%32768u)*65536u);
        p[1]=static_cast<int>((2*(n%10)+1)*2147483647LL/20);
        p[2]=block<64?0:block<128?INT_MAX:static_cast<int>((block*127u%32768u)*65536u);
        p[3]=block%2?0:INT_MAX;
        std::fill(pitch,pitch+64,bank_pitch[3+n]+123);
        render(voices[n],shared,n+1,out,p,pitch);
        require(shared.voices.get(n+1,voices[n].lease)!=nullptr,"24 active note leases survive changing controls");
        for(int value:out)require(value>=-805306368&&value<=805306368,"24-note changing-control headroom");
    }
    for(unsigned type=0;type<10;++type){
        DisplayState state={};troupe::Frame frame={};opxy::draw::Input input={};
        input.slots[0]=INT_MAX/2;input.slots[1]=static_cast<int>((2*type+1)*2147483647LL/20);
        input.slots[2]=INT_MAX;input.slots[3]=INT_MAX/2;input.held_notes=3;
        display_reset(state);
        int previous[5]={};
        for(unsigned tick=0;tick<32;++tick){
            input.elapsed=tick?17:0;display(state,input,frame);
            require(opxy::draw::valid(frame),"Every entrance, face and silhouette frame is valid");
            for(unsigned s=0;s<5;++s){const auto& node=frame.nodes[face_node+s];
                if(tick)require(node.y<=previous[s],"Center-out entrance has no bounce");previous[s]=node.y;
                const unsigned body=cast_members[state.selected_type][s];
                const auto& silhouette=frame.nodes[body_node+s*voice_count+body];
                require(silhouette.x==node.x&&silhouette.y==node.y&&silhouette.scale==node.scale,"Actual contributing cast silhouettes share the face pose");
            }
        }
        require(state.eye_hold_age>=184,"Sustained chords retain delayed eye closure");
        input.reduced_motion=1;display(state,input,frame);require(state.phase==0,"Reduced motion disables sway");
        input.held_notes=0;display(state,input,frame);require(state.eye_hold_age==0,"Release clears held-note pose");
    }
    std::printf("PASS: %lu deterministic bounded voice samples; 24-note live ramps; one audio stream; all ten visual entrances and contributing identities.\n",samples);
}
