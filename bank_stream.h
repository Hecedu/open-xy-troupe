#pragma once
#include <opxy/audio.hpp>
namespace recital {
// Four wave corners resume their exact residual stream during a morph.
// Scalar state only; no pointers, cache ownership, allocation or extra waves.
struct BankStream {
    opxy::u32 control;
    opxy::i32 history[4], coefficients[4];
};
}
