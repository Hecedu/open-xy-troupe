#pragma once
#include <opxy/draw.hpp>
#include "voice_types.h"
namespace recital {
// SDK drawing frame: mutable outlines, points, nodes, text slots, palette entries.
// The visual ensemble is independent of the three audio singers.
const unsigned display_singers=5;
// Authored outlines keep five matching mouths within the four-path SDK limit.
const unsigned mouth_steps=17;
const unsigned body_node=display_singers*mouth_steps;
const unsigned open_eyes_node=body_node+display_singers*voice_count;
const unsigned closed_eyes_node=open_eyes_node+display_singers;
const unsigned projection_node=closed_eyes_node+display_singers;
const unsigned background_node=projection_node+display_singers;
const unsigned face_node=background_node+1;
using Frame = opxy::draw::Frame<0,0,face_node+display_singers,0,16>;
struct DisplayState {
    opxy::draw::NoteTracker notes;
    opxy::u32 phase, previous_event;
    opxy::i32 pulse_age, eye_hold_age;
    opxy::u32 selected_type;
    opxy::u32 look_weights[voice_count], look_start[voice_count];
    opxy::u32 look_initialized, look_track;
    opxy::i32 look_age;
    opxy::u32 startup_track, startup_initialized;
    opxy::i32 startup_age;
};
}
