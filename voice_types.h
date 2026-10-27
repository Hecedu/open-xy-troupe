#pragma once
#include <opxy/audio.hpp>
namespace troupe {
const unsigned voice_count=10;
// Female/male pairs: young, bright/light, adult, textured, opera.
// Internal identities of the original six remain unchanged; new ones append.
const unsigned voice_order[voice_count]={5,0,8,9,1,2,6,7,3,4};
inline unsigned voice_type(opxy::i32 parameter) {
    const opxy::u32 value=parameter<0?0:static_cast<opxy::u32>(parameter)>>16;
    return voice_order[(value*voice_count)>>15];
}
}
