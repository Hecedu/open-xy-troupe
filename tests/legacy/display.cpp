#include <cstdio>
#include <cstdlib>
#include <climits>
#include "../display.cpp"

static_assert(troupe::max_singers == 3, "Audio still renders three singers");
static_assert(troupe::Frame::path_count == 0 && troupe::Frame::node_count == 156,
              "Five visual characters use the native drawing contract");
static void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static bool hidden(const opxy::draw::Node& node) {
    return node.x == opxy::draw::hidden_x * 1024 && node.y == opxy::draw::hidden_y * 1024;
}
static_assert(sizeof(troupe::DisplayState) == 140, "Declared held-note state budget");
static void settled_reset(troupe::DisplayState& state) {
    troupe::display_reset(state);
    state.startup_initialized=1; state.startup_track=0; state.startup_age=troupe::startup_end;
}
static void eyes(const troupe::Frame& frame, bool singing) {
    require(opxy::draw::valid(frame), "Every frame satisfies the native drawing contract");
    for (unsigned s = 0; s < 5; ++s) {
        require(hidden(frame.nodes[troupe::open_eyes_node+s]) == singing, "Dot eyes follow held notes");
        require(hidden(frame.nodes[troupe::closed_eyes_node+s]) != singing, "Closed eyes follow held notes");
    }
}
static bool same_transform(const opxy::draw::Node& a,const opxy::draw::Node& b) {
    return a.x==b.x && a.y==b.y && a.scale==b.scale && a.cos==b.cos && a.sin==b.sin;
}
static void voice_position_checks() {
    // Switch live in idle, during the eye delay, after closure and with reduced motion.
    for (unsigned mode=0;mode<4;++mode) {
        troupe::DisplayState state={}; troupe::Frame frame={}; opxy::draw::Input input={};
        input.slots[0]=INT_MAX/2; input.slots[1]=322122547; input.slots[2]=INT_MAX; input.slots[3]=INT_MAX/2;
        settled_reset(state); troupe::display(state,input,frame);
        if(mode) {
            input.held_notes=1; ++input.note_serial; input.reduced_motion=mode==3;
            troupe::display(state,input,frame);
            if(mode>=2) {
                input.elapsed=102;
                troupe::display(state,input,frame); troupe::display(state,input,frame);
            }
        }
        const troupe::Frame reference=frame;
        const int reference_age=state.eye_hold_age;
        const unsigned reference_phase=state.phase;
        input.elapsed=0;
        for(unsigned step=0;step<20;++step) {
            const unsigned type=step<10?step:19-step;
            input.slots[1]=static_cast<int>((type*2+1)*(2147483647.0/20));
            troupe::display(state,input,frame); eyes(frame,mode>=2);
            for(unsigned singer=0;singer<5;++singer) {
                const unsigned body=85+singer*troupe::voice_count, mouth=singer*17+8;
                const unsigned eye=(mode>=2?troupe::closed_eyes_node:troupe::open_eyes_node)+singer;
                require(same_transform(frame.nodes[troupe::face_node+singer],reference.nodes[troupe::face_node+singer]) &&
                        frame.nodes[troupe::face_node+singer].shade==reference.nodes[troupe::face_node+singer].shade,
                        "Voice changes preserve the solid face origin and brightness at every arc depth");
                for(unsigned look=0;look<troupe::voice_count;++look) if(!hidden(frame.nodes[body+look]))
                    require(same_transform(frame.nodes[body+look],reference.nodes[body]),
                            "Every dissolving silhouette shares the original face/body pose");
                require(same_transform(frame.nodes[mouth],reference.nodes[mouth]) &&
                        same_transform(frame.nodes[eye],reference.nodes[eye]) &&
                        same_transform(frame.nodes[troupe::projection_node+singer],reference.nodes[troupe::projection_node+singer]),
                        "Eyes, mouth and projection keep their exact pose across every voice change");
            }
            require(state.eye_hold_age==reference_age && state.phase==reference_phase,
                    "Voice changes do not restart the eye delay or sway");
        }
    }
}
static void voice_transition_checks() {
    troupe::DisplayState state={}; troupe::Frame frame={}; opxy::draw::Input input={};
    input.slots[0]=INT_MAX/2; input.slots[1]=322122547; input.slots[2]=INT_MAX; input.slots[3]=INT_MAX/2;
    settled_reset(state); troupe::display(state,input,frame);
    const troupe::Frame original=frame;
    require(state.look_weights[0]==1024,"Initial character appears immediately without a startup fade");
    input.slots[1]=107374182; // 1, artwork 5.
    troupe::display(state,input,frame);
    require(state.look_weights[0]==1024 && state.look_weights[5]==0 &&
            same_transform(frame.nodes[85],original.nodes[85]) && frame.nodes[85].shade==12,
            "A new selection starts from the previous rendered appearance");
    unsigned prior_old=12,prior_new=0; bool overlapped=false;
    for(unsigned tick=0;tick<14;++tick) {
        input.elapsed=17; troupe::display(state,input,frame); eyes(frame,false);
        require(opxy::draw::valid(frame),"Dissolve frames satisfy the native retained drawing contract");
        for(unsigned singer=0;singer<5;++singer) {
            const unsigned old=85+singer*troupe::voice_count, next=old+5;
            const unsigned old_shade=hidden(frame.nodes[old])?0:frame.nodes[old].shade;
            const unsigned new_shade=hidden(frame.nodes[next])?0:frame.nodes[next].shade;
            require(old_shade<=prior_old && new_shade>=prior_new,"Outgoing look dims and incoming look brightens monotonically");
            require(old_shade||new_shade,"The character never disappears during a dissolve");
            overlapped=overlapped||(old_shade&&new_shade);
            require(same_transform(frame.nodes[troupe::face_node+singer],original.nodes[troupe::face_node+singer]) &&
                    frame.nodes[troupe::face_node+singer].shade==original.nodes[troupe::face_node+singer].shade &&
                    same_transform(frame.nodes[troupe::open_eyes_node+singer],original.nodes[troupe::open_eyes_node+singer]) &&
                    same_transform(frame.nodes[singer*17+8],original.nodes[singer*17+8]),
                    "Face outline, eyes and mouth remain steady throughout the dissolve");
            if(singer==4) {prior_old=old_shade;prior_new=new_shade;}
        }
    }
    require(overlapped,"A transition blends both looks instead of switching at one midpoint");
    require(state.look_weights[5]==1024 && hidden(frame.nodes[85]) && frame.nodes[90].shade==12,
            "The dissolve settles to exactly the selected character after about 220 ms");
    input.slots[1]=322122547; input.elapsed=0; troupe::display(state,input,frame);
    input.elapsed=68; troupe::display(state,input,frame);
    unsigned previous[troupe::voice_count];
    for(unsigned look=0;look<troupe::voice_count;++look) previous[look]=state.look_weights[look];
    input.slots[1]=INT_MAX/2; input.elapsed=0; troupe::display(state,input,frame);
    for(unsigned look=0;look<troupe::voice_count;++look)
        require(state.look_weights[look]==previous[look],"Interrupting a dissolve preserves the current mixture exactly");
    // Missing note activity must not restart a display-only character transition.
    input.held_notes=opxy::draw::notes_unavailable;
    input.reduced_motion=1;
    for(unsigned tick=0;tick<80;++tick) {
        const unsigned type=tick%troupe::voice_count;
        input.slots[1]=static_cast<int>((type*2+1)*(2147483647.0/20));
        input.elapsed=0; troupe::display(state,input,frame);
        for(unsigned look=0;look<troupe::voice_count;++look)
            require(state.look_weights[look]==previous[look],"Rapid retargeting does not jump to a stale start pose");
        input.elapsed=34; troupe::display(state,input,frame);
        unsigned total=0;
        for(unsigned look=0;look<troupe::voice_count;++look) {
            require(state.look_weights[look]<=1024,"Every transition weight stays bounded");
            total+=state.look_weights[look]; previous[look]=state.look_weights[look];
        }
        require(total==1024 && state.phase==0 && opxy::draw::valid(frame),
                "Rapid reduced-motion transitions preserve exact mixture weight without spatial motion");
    }
    const unsigned target=state.selected_type;
    for(unsigned tick=0;tick<14;++tick) {input.elapsed=17;troupe::display(state,input,frame);}
    require(state.look_weights[target]==1024,"Rapid changes settle correctly when the control stops");
    input.track=1; input.slots[1]=322122547; troupe::display(state,input,frame);
    require(state.look_weights[0]==1024,"Track changes establish the current look without a stale crossfade");
    settled_reset(state); input.slots[1]=107374182; troupe::display(state,input,frame);
    require(state.look_weights[5]==1024,"Reset establishes the selected look without an initial dissolve");
}

static void startup_checks() {
    for(unsigned look=0;look<troupe::voice_count;++look) {
        troupe::DisplayState state={}; troupe::Frame frame={}; opxy::draw::Input input={};
        input.slots[1]=static_cast<int>((look*2+1)*2147483647LL/20);
        input.slots[0]=INT_MAX/2; input.slots[2]=INT_MAX; input.slots[3]=INT_MAX;
        troupe::display_reset(state); troupe::display(state,input,frame);
        require(state.startup_age==0,"First rendered frame begins the entrance without consuming a stale delta");
        int last_y[5];
        for(unsigned s=0;s<5;++s) last_y[s]=frame.nodes[troupe::face_node+s].y;
        for(unsigned tick=0;tick<12;++tick) {
            input.elapsed=17; troupe::display(state,input,frame);
            require(opxy::draw::valid(frame),"Every entrance pose fits the native retained drawing bounds");
            for(unsigned s=0;s<5;++s) {
                const auto& face=frame.nodes[troupe::face_node+s];
                require(face.y<=last_y[s],"Singers arrive smoothly without a vertical rebound");last_y[s]=face.y;
                require(same_transform(face,frame.nodes[troupe::body_node+s*troupe::voice_count+state.selected_type]),"Entrance moves the whole silhouette with the face");
                const auto& mouth=frame.nodes[s*17+8];
                require(mouth.x==face.x&&mouth.y==face.y-10*face.scale&&mouth.scale==face.scale,"Mouth stays attached during entrance");
                require(same_transform(face,frame.nodes[troupe::open_eyes_node+s]),"Eyes stay attached during entrance");
            }
        }
        require(state.startup_age==204 && state.startup_age<troupe::startup_end,
                "Entrance is still in progress immediately before its 200 ms deadline");
        input.elapsed=1; troupe::display(state,input,frame);
        require(troupe::startup_end==205 && state.startup_age==troupe::startup_end,
                "Full entrance including stagger settles at 200 ms to Q10 precision");
        troupe::DisplayState reference=state; reference.startup_age=troupe::startup_end;
        troupe::Frame settled={}; input.elapsed=0; troupe::display(reference,input,settled);
        require(__builtin_memcmp(&settled,&frame,sizeof(frame))==0,"Finished entrance has no remaining pose offset");
        input.slots[1]=INT_MAX/2; input.held_notes=1; ++input.note_serial;
        troupe::display(state,input,frame);
        require(state.startup_age==troupe::startup_end,"Notes and Voice changes do not replay the entrance");
        input.track=1; troupe::display(state,input,frame);
        require(state.startup_age==0,"A newly selected track starts a fresh entrance");
        input.reduced_motion=1; troupe::display(state,input,frame);
        require(state.startup_age==troupe::startup_end,"Reduced motion immediately presents the settled arc");
        input.reduced_motion=0; troupe::display(state,input,frame);
        require(state.startup_age==troupe::startup_end,"Turning reduced motion off does not replay an entrance");
        troupe::display_reset(state); input.elapsed=INT_MAX; troupe::display(state,input,frame);
        require(state.startup_age==0,"Activation reset restarts at the first pose even after a long pause");
        troupe::display(state,input,frame); require(state.startup_age==102,"Late entrance frames use the existing elapsed clamp");
        input.elapsed=INT_MIN; troupe::display(state,input,frame);require(state.startup_age==102,"Negative elapsed cannot rewind entrance");
    }
}

int main() {
    startup_checks();
    voice_position_checks();
    voice_transition_checks();
    for (unsigned type = 0; type < troupe::voice_count; ++type) {
        troupe::DisplayState state = {};
        troupe::Frame frame = {};
        opxy::draw::Input input = {};
        input.track = 0; input.note_serial = 45;
        input.slots[0] = INT_MAX / 2;
        input.slots[1] = static_cast<int>((type * 2 + 1) * (2147483647.0 / 20));
        input.slots[2] = INT_MAX; input.slots[3] = INT_MAX / 2;
        settled_reset(state);
        troupe::display(state,input,frame); eyes(frame,false);
        for (unsigned s = 0; s < 5; ++s) {
            for (unsigned look = 0; look < troupe::voice_count; ++look)
                require(hidden(frame.nodes[85+s*troupe::voice_count+look]) == (look != state.selected_type),
                        "Every character shows only the selected look");
            for (unsigned mouth = 0; mouth < 17; ++mouth)
                require(hidden(frame.nodes[s*17+mouth]) == (mouth != 8),
                        "All five mouths show the selected vowel outline");
        }
        // Judge spacing along the ground arc, independently of character height.
        const int foot_height=29;
        int baseline_y[5], original_x[5];
        long long feet[5];
        for (unsigned s = 0; s < 5; ++s) {
            const auto& body=frame.nodes[85+s*troupe::voice_count+state.selected_type];
            baseline_y[s]=body.y; original_x[s]=body.x;
            feet[s]=body.y+foot_height*body.scale;
        }
        require(original_x[1]+original_x[2]==2*original_x[0] &&
                original_x[3]+original_x[4]==2*original_x[0] &&
                feet[1]==feet[2] && feet[3]==feet[4], "Arc is mirrored around its front singer");
        long long gaps[4];
        const unsigned order[5]={3,1,0,2,4};
        for (unsigned i=0;i<4;++i) {
            const unsigned a=order[i],b=order[i+1];
            const long long dx=original_x[a]-original_x[b],dy=feet[a]-feet[b];
            gaps[i]=dx*dx+dy*dy;
        }
        for (unsigned i=1;i<4;++i)
            require(std::llabs(gaps[i]-gaps[0])<256*1024,
                    "Adjacent singers have equal chord distances along the circular arc to Q10 precision");
        input.note_serial++; input.held_notes=1;
        input.elapsed = 17;
        bool moved = false;
        int previous[5];
        for (unsigned s=0;s<5;++s) previous[s]=original_x[s];
        const int expected_scale[5] = {1920,1787,1787,1408,1408};
        for (unsigned f = 0; f < 900; ++f) {
            const bool closed=f*17>=184;
            troupe::display(state,input,frame); eyes(frame,closed);
            for (unsigned s = 0; s < 5; ++s) {
                const unsigned active_eyes=(closed?troupe::closed_eyes_node:troupe::open_eyes_node)+s;
                const unsigned body = 85+s*troupe::voice_count+state.selected_type;
                const opxy::draw::Node& node = frame.nodes[body];
                require(node.y == baseline_y[s], "Sway preserves the U-shaped depth positions");
                require(node.scale == expected_scale[s] && node.cos == 1024 && node.sin == 0,
                        "Sway keeps each singer's perspective scale without rotation");
                require(std::abs(node.x-previous[s]) <= 300, "Horizontal sway stays smooth");
                moved = moved || node.x != previous[s]; previous[s] = node.x;
                require(node.x == frame.nodes[s*17+8].x && node.y-10*node.scale == frame.nodes[s*17+8].y,
                        "Mouth follows the body with the same perspective-scaled offset");
                require(frame.nodes[s*17+8].scale == node.scale && frame.nodes[active_eyes].scale == node.scale &&
                        frame.nodes[troupe::projection_node+s].scale == node.scale, "All parts share the singer's perspective scale");
                require(node.x == frame.nodes[active_eyes].x && node.y == frame.nodes[active_eyes].y, "Eyes follow the body");
                require(node.x == frame.nodes[troupe::projection_node+s].x, "Projection follows the body");
            }
        }
        require(frame.nodes[85+state.selected_type].y > frame.nodes[85+troupe::voice_count+state.selected_type].y &&
                frame.nodes[85+troupe::voice_count+state.selected_type].y > frame.nodes[85+3*troupe::voice_count+state.selected_type].y,
                "The center stands in front of the two progressively higher pairs");
        require(frame.nodes[85+state.selected_type].scale > frame.nodes[85+troupe::voice_count+state.selected_type].scale &&
                frame.nodes[85+troupe::voice_count+state.selected_type].scale > frame.nodes[85+3*troupe::voice_count+state.selected_type].scale &&
                frame.nodes[85+3*troupe::voice_count+state.selected_type].scale > 1024,
                "All singers are enlarged and shrink as they recede up the screen");
        require(moved, "Long holds keep swaying without further onsets");
        // Adding/releasing chord notes must not reset or end the held pose.
        unsigned phase = state.phase;
        input.note_serial++; input.held_notes=3;
        troupe::display(state,input,frame); eyes(frame,true);
        require(state.phase == phase+17 && state.pulse_age == 0, "Chord onset keeps continuous sway");
        phase=state.phase; input.held_notes=2; input.note_off_serial++;
        troupe::display(state,input,frame); eyes(frame,true);
        require(state.phase == phase+17, "Partial chord release keeps continuous sway");
        phase=state.phase; input.note_off_serial++; // A release/restrike coalesced into one frame.
        input.note_serial++;
        troupe::display(state,input,frame); eyes(frame,true);
        require(state.phase == phase+17, "Coalesced release/restrike keeps the held pose");
        input.held_notes=0; input.note_off_serial+=2;
        troupe::display(state,input,frame); eyes(frame,false);
        require(frame.nodes[85+state.selected_type].x == 240*1024 && state.phase == 0,
                "Last release immediately returns to rest");
        input.note_serial++; troupe::display(state,input,frame); eyes(frame,false);
        require(state.phase == 0, "Completed taps between frames do not leave a timed pose");
        input.held_notes=1; troupe::display(state,input,frame); eyes(frame,false);
        input.reduced_motion=1;
        input.elapsed=102; troupe::display(state,input,frame); eyes(frame,false);
        input.elapsed=82; troupe::display(state,input,frame); eyes(frame,true);
        input.elapsed=17;
        require(state.phase == 0 && frame.nodes[85+state.selected_type].x == 240*1024,
                "Reduced motion stops sway while keeping closed eyes");
        input.held_notes=0; input.note_off_serial++;
        troupe::display(state,input,frame); eyes(frame,false);
        input.held_notes=1; input.note_serial++;
        troupe::display(state,input,frame); eyes(frame,false);
        input.reduced_motion=0; troupe::display(state,input,frame); eyes(frame,false);
        input.track=1; input.note_serial=99; input.held_notes=0;
        troupe::display(state,input,frame); eyes(frame,false);
        input.track=2; input.held_notes=2;
        troupe::display(state,input,frame); eyes(frame,false);
        require(state.pulse_age == troupe::idle_age && state.phase == 17,
                "Switching to a held track reads its state without an onset pulse");
        input.track=8; troupe::display(state,input,frame); eyes(frame,false);
        input.track=0; input.held_notes=0; input.note_serial=UINT_MAX; input.note_off_serial=UINT_MAX;
        troupe::display(state,input,frame);
        input.note_serial=0; input.held_notes=1; troupe::display(state,input,frame); eyes(frame,false);
        input.note_off_serial=0; input.held_notes=0; troupe::display(state,input,frame); eyes(frame,false);
        input.held_notes=128; troupe::display(state,input,frame); eyes(frame,false);
        input.held_notes=opxy::draw::notes_unavailable;
        troupe::display(state,input,frame); eyes(frame,false);
        require(state.phase == 0 && !state.notes.initialized, "Unavailable activity clears the held baseline");
        input.held_notes=1; troupe::display(state,input,frame); eyes(frame,false);
        require(state.pulse_age == troupe::idle_age, "Restored activity does not fabricate an onset");
        input.elapsed=INT_MAX; phase=state.phase; troupe::display(state,input,frame); eyes(frame,false);
        require(state.phase == phase+102, "Late frames clamp elapsed time");
        input.elapsed=INT_MIN; phase=state.phase; troupe::display(state,input,frame); eyes(frame,false);
        require(state.phase == phase, "Negative elapsed time does not advance sway");
        settled_reset(state); troupe::display(state,input,frame); eyes(frame,false);
        require(state.phase == 0 && state.pulse_age == troupe::idle_age,
                "Reset while held starts the eye delay without fabricating an onset");
        input.held_notes=0; troupe::display(state,input,frame); eyes(frame,false);
        input.elapsed=0; input.slots[3]=0;
        unsigned previous_shades[5] = {};
        for (unsigned step = 0; step <= 16; ++step) {
            input.slots[2] = static_cast<int>(step * (2147483647.0 / 16));
            troupe::display(state,input,frame); eyes(frame,false);
            for (unsigned s = 0; s < 5; ++s) {
                const unsigned shade = frame.nodes[85+s*troupe::voice_count+state.selected_type].shade;
                require(shade >= previous_shades[s], "Ensemble brightens each whole character progressively");
                previous_shades[s] = shade;
                if (step == 0 && s)
                    require(shade == 3 && frame.nodes[troupe::open_eyes_node+s].shade == 2 && frame.nodes[s*17+8].shade == 2,
                            "Solo keeps all four companion silhouettes dim");
                if (step == 16)
                    require(shade == 12 && frame.nodes[troupe::open_eyes_node+s].shade == 13 && frame.nodes[s*17+8].shade == 15,
                            "Full Ensemble lights all five complete characters");
                require(hidden(frame.nodes[troupe::projection_node+s]), "Minimum Effort hides every projection ray");
            }
        }
        // A gate shorter than 180 ms must never blink, even across many arp cycles.
        settled_reset(state); input.held_notes=0; input.elapsed=0;
        input.reduced_motion=0; troupe::display(state,input,frame); eyes(frame,false);
        const auto advance=[&](int duration,bool closed) {
            while(duration>0) {
                input.elapsed=duration>102?102:duration;
                troupe::display(state,input,frame); duration-=input.elapsed;
            }
            eyes(frame,closed);
        };
        const int short_gates[]={16,32,64,96,128,183};
        for (unsigned rate=0;rate<6;++rate) for (unsigned repeat=0;repeat<32;++repeat) {
            input.elapsed=0; input.held_notes=1; ++input.note_serial;
            troupe::display(state,input,frame); eyes(frame,false);
            advance(short_gates[rate],false);
            input.held_notes=0; ++input.note_off_serial; input.elapsed=1;
            troupe::display(state,input,frame); eyes(frame,false);
        }
        input.elapsed=0; input.held_notes=1; ++input.note_serial;
        troupe::display(state,input,frame); eyes(frame,false);
        advance(183,false); advance(1,true);
        input.held_notes=2; ++input.note_serial; input.elapsed=17;
        troupe::display(state,input,frame); eyes(frame,true);
        input.held_notes=1; ++input.note_off_serial;
        troupe::display(state,input,frame); eyes(frame,true);
        input.held_notes=0; ++input.note_off_serial;
        troupe::display(state,input,frame); eyes(frame,false);
        input.held_notes=1; input.elapsed=0; ++input.note_serial;
        troupe::display(state,input,frame); eyes(frame,false);
        input.reduced_motion=1; advance(184,true);
        require(state.phase==0,"Reduced motion preserves delayed eye feedback without sway");

    }
    std::puts("PASS: five visual characters, three audio singers, all six looks, shared vowels, Ensemble fades, SDK 1.1 held-note poses, 15-second holds, synchronized sway, chords, partial/final releases, track changes, reset, reduced motion, unavailable activity, serial wrap, 192 short arp gates without blinking the 180 ms eye threshold and interruptible 220 ms character dissolves with steady faces.");
}
