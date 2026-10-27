#include "display.h"
#include "core.h"
#include "bank.h"
#include "voice_types.h"
#include "effort.h"
namespace recital {
namespace {
using opxy::i32;
using opxy::u32;
using namespace opxy::draw;
const i32 idle_age=2048;
const i32 eye_hold_delay=184; // About 180 ms of continuously observed held notes.
const i32 look_duration=225; // About 220 ms; palette-only dissolve, also with reduced motion.
const i32 startup_duration=171; // About 167 ms travel; outer pair completes at 200 ms.
const i32 startup_delay[display_singers]={0,17,17,34,34};
const i32 startup_end=startup_duration+34;
const i32 positions[display_singers]={2,1,3,0,4};
// Feet sit at equal angular (and arc-length) intervals on one circular U.
// Circle: center (240, -47.1111), radius 251.1111; Q10 samples at
// 0, half and full endpoint angle. The front baseline is 204, the back 132.
const i32 arc_x[3]={0,97361,180224};
const i32 arc_y[3]={208896,189751,135168};
// Every authored silhouette shares this ground offset from the face origin.
// Voice selection changes the outline, never the face's screen height.
const i32 feet_y=29;
const i32 stagger[display_singers]={0,18,11,29,23};
// Interpolate the periodic table instead of stepping through whole samples.
inline i32 smooth_sine(u32 phase){
    phase&=65535;
    const u32 index=phase>>8, fraction=phase&255;
    return sine[index]+(sine[index+1]-sine[index])*static_cast<i32>(fraction)/256;
}
inline void update_look(DisplayState& state,u32 requested,u32 track,i32 elapsed) {
    if(!state.look_initialized||state.look_track!=track) {
        state.look_initialized=1; state.look_track=track;
        state.selected_type=requested; state.look_age=look_duration;
        for(u32 look=0;look<voice_count;++look)
            state.look_start[look]=state.look_weights[look]=look==requested?unit:0;
    } else if(requested!=state.selected_type) {
        // Interrupt from the exact currently drawn mix, including rapid scrubbing.
        for(u32 look=0;look<voice_count;++look) state.look_start[look]=state.look_weights[look];
        state.selected_type=requested; state.look_age=0;
    } else if(state.look_age<look_duration) {
        state.look_age=clamp(state.look_age+elapsed,0,look_duration);
        const i32 t=state.look_age*unit/look_duration;
        const i32 ease=t*t/unit*(3*unit-2*t)/unit;
        u32 remaining=unit;
        for(u32 look=0;look<voice_count;++look) if(look!=requested) {
            state.look_weights[look]=state.look_start[look]*static_cast<u32>(unit-ease)/unit;
            remaining-=state.look_weights[look];
        }
        state.look_weights[requested]=remaining;
    }
}
}
inline void display_reset(DisplayState& state) {
    state.notes.reset();
    state.phase=state.previous_event=0;
    state.pulse_age=idle_age;
    state.eye_hold_age=0;
    state.selected_type=0;
    state.look_initialized=state.look_track=0;
    state.look_age=look_duration;
    state.startup_track=state.startup_initialized=0; state.startup_age=0;
    for(u32 look=0;look<voice_count;++look) state.look_start[look]=state.look_weights[look]=0;
}
inline void display(DisplayState& state,const opxy::draw::Input& input,Frame& frame) {
    using namespace opxy::draw;
    const i32 elapsed=clamp(input.elapsed,0,102);
    if(!state.startup_initialized||state.startup_track!=input.track) {
        state.startup_initialized=1; state.startup_track=input.track; state.startup_age=0;
    } else state.startup_age=clamp(state.startup_age+elapsed,0,startup_end);
    if(input.reduced_motion) state.startup_age=startup_end;
    const bool snapshot=!state.notes.initialized||state.notes.track!=input.track;
    const auto events=state.notes.update(input);
    const bool singing=input.track<8&&input.held_notes<=128&&input.held_notes!=0;
    // Short arpeggiator gates never close the eyes. Chord changes do not
    // restart the delay while at least one note remains continuously held.
    if(snapshot||!singing||events.hold_started) state.eye_hold_age=0;
    else state.eye_hold_age=clamp(state.eye_hold_age+elapsed,0,eye_hold_delay);
    const bool eyes_closed=singing&&state.eye_hold_age>=eye_hold_delay;
    if(snapshot||!singing||input.reduced_motion) state.pulse_age=idle_age;
    else {
        state.pulse_age=clamp(state.pulse_age+elapsed,0,idle_age);
        if(events.hold_started||input.note_on_serial!=state.previous_event) state.pulse_age=0;
    }
    state.previous_event=input.note_on_serial;
    if(snapshot||events.hold_started||events.hold_ended) state.phase=0;
    if(singing&&!input.reduced_motion) state.phase+=static_cast<u32>(elapsed);
    else state.phase=0;
    const i32 vowel=clamp(input.slots[0],0,2147483647)/65536;
    update_look(state,voice_type(input.slots[1]),input.track,elapsed);
    const i32 ensemble=input.slots[2]>=2147418112?32768:clamp(input.slots[2],0,2147483647)/65536;
    const i32 vocal_effort=effort_value(input.slots[3]);
    const u32 mouth_shape=static_cast<u32>((vowel*(mouth_steps-1)+16384)/32768);
    place(frame,background_node,0,0,unit,0,keep);
    for(u32 s=0;s<display_singers;++s){
        const i32 weight=s==0?32768:clamp(ensemble*(display_singers-1)-static_cast<i32>(s-1)*32768,0,32768);
        const i32 slot=positions[s], distance=slot<2?2-slot:slot-2;
        const i32 x=240*unit+(slot<2?-arc_x[distance]:arc_x[distance]);
        const i32 scale=1408+(arc_y[distance]-132*unit)*512/(72*unit);
        const i32 age=state.pulse_age-stagger[s];
        const bool moving=singing&&!input.reduced_motion&&weight>0;
        const i32 pulse=!moving||age<0||age>420?0:(420-age)*1024/420;
        // 1.5 Hz horizontal sway, with a 125 ms smoothstep entrance.
        // Every part shares this pose; Y, scale and rotation stay fixed.
        const u32 phase=(state.phase*96u+s*9472u)&65535;
        const i32 ramp=static_cast<i32>(state.phase<128?state.phase:128)*8;
        const i32 ease=ramp*ramp/1024*(3072-2*ramp)/1024;
        const i32 shake_x=moving?smooth_sine(phase)*1280/32768*weight/32768*ease/1024:0;
        const i32 travel=clamp(state.startup_age-startup_delay[s],0,startup_duration);
        const i32 t=travel*unit/startup_duration;
        // Cubic deceleration lands exactly, without a bounce or a residual offset.
        const i32 remaining=unit-t;
        const i32 arrival=unit-remaining*remaining/unit*remaining/unit;
        // Rise from below the screen while fanning out to the circular ground arc.
        // All artwork shares one origin, keeping eyes, mouth and silhouettes aligned.
        const i32 gather=(x-240*unit)/4;
        const i32 startup_x=-gather*(unit-arrival)/unit;
        const i32 startup_y=(300*unit-arc_y[distance])*(unit-arrival)/unit;
        const i32 px=x+shake_x+startup_x, py=arc_y[distance]-feet_y*scale+startup_y;
        const u32 shade=static_cast<u32>(3+weight*9/32768+pulse*weight/32768/512);
        for(u32 look=0;look<voice_count;++look) {
            const u32 body=body_node+s*voice_count+look;
            const u32 body_shade=(shade*state.look_weights[look]+unit/2)/unit;
            if(body_shade)place(frame,body,px,py,scale,body_shade,stroke);
            else hide(frame,body);
        }
        // Keep the identical face outline solid while hair and clothing dissolve.
        place(frame,face_node+s,px,py,scale,shade,stroke);
        for(u32 shape=0;shape<mouth_steps;++shape) {
            const u32 mouth_node=s*mouth_steps+shape;
            if(shape==mouth_shape)
                place(frame,mouth_node,px,py-10*scale,scale,static_cast<u32>(2+weight*13/32768),stroke);
            else hide(frame,mouth_node);
        }
        if(eyes_closed) {
            hide(frame,open_eyes_node+s);
            place(frame,closed_eyes_node+s,px,py,scale,static_cast<u32>(2+weight*11/32768),stroke);
        } else {
            place(frame,open_eyes_node+s,px,py,scale,static_cast<u32>(2+weight*11/32768),fill_and_stroke);
            hide(frame,closed_eyes_node+s);
        }
        if(vocal_effort<128||weight==0) hide(frame,projection_node+s);
        else {
            const i32 flicker=moving?(smooth_sine(phase+16384)+32768)/16384:0;
            const i32 shade=clamp((vocal_effort*weight/32768)*(5+flicker)/32768+pulse/256,0,10);
            place(frame,projection_node+s,px,py,scale,static_cast<u32>(shade),stroke);
        }
    }
}
}
