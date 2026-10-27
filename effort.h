#pragma once
#include <opxy/audio.hpp>
namespace troupe { inline opxy::i32 effort_value(opxy::i32 p){return p<0?0:p>=2147418112?32768:p/65536;} }
