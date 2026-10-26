#pragma once
#include <opxy/audio.hpp>
namespace recital {
struct CharacterExpression {
    opxy::u32 attack_step, vibrato_delay, vibrato_fade_step, vibrato_rate;
    opxy::i32 vibrato_depth;
};
// Attack/fade steps are Q15 per 64-frame block. Vibrato rate is phase per
// block; depth is a Q16 pitch ratio (40 ~ 1 cent, 850 ~ 22 cents).
// Internal identity order: Bart, Emily, Luis, Tamaki, Bertram, Mandy,
// Mabel, Rocco, Zelda, Pip. Original expression values remain exact.
// Attacks: 41, 70, 93, 81, 104, 26 ms. Note-start timing is latched;
// changing character on a held note retains phase and smooths depth.
const CharacterExpression character_expression[10]={
    {(32768u+27)/28,552,(32768u+206)/207,32411907u,40},
    {(32768u+47)/48,296,(32768u+206)/207,31788601u,280},
    {(32768u+63)/64,345,(32768u+275)/276,27425460u,160},
    {(32768u+55)/56,104,(32768u+185)/186,36151743u,700},
    {(32768u+71)/72,138,(32768u+261)/262,28048766u,850},
    {(32768u+17)/18,483,(32768u+206)/207,34905131u,120},
    {(32768u+57)/58,380,(32768u+275)/276,24932236u,380},
    {(32768u+23)/24,450,(32768u+240)/241,26178848u,100},
    {(32768u+15)/16,310,(32768u+165)/166,34281825u,180},
    {(32768u+49)/50,600,(32768u+275)/276,29918683u,25}
};
// Relative aspiration follows the authored airy/relaxed/operatic identities.
// Q8 gains, in the same internal character order as the expression table.
const opxy::i32 character_air[10]={384,320,160,112,96,192,352,288,144,240};
}
