#pragma once
#include <opxy/audio.hpp>
#include "bank_stream.h"
namespace recital {
// Authored periodic vocal waves, generated offline. Layout: type, pitch, vowel.
const unsigned bank_pitch_count=29, bank_vowel_count=5, bank_frames=256;
const opxy::u32 bank_pitch[29]={3185014,3787642,4504291,5356535,6370029,7575284,9008582,10713070,12740059,15150569,18017164,21426140,25480118,30301138,36034329,42852281,50960237,60602277,72068659,85704562,101920475,121204555,144137319,171409125,203840951,242409110,288274638,342818251,407681903};
#include "bank_packed.h"
// A 32-bit lookahead needs at most two immutable words. Guard words follow
// the packed bank; bit offsets remove per-block padding and unused seeds.
inline opxy::u32 bank_window(unsigned bit) {
    const unsigned at=bit>>5,shift=bit&31;
    opxy::u32 value=bank_words[at]>>shift;
    if(shift)value|=bank_words[at+1]<<(32-shift);
    return value;
}
const unsigned char bank_byte_zeros[256]={
    8,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
    5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
    6,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
    5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
    7,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
    5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
    6,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
    5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0
};
// Exact bounded byte lookup: one conditional for every Rice quotient.
inline unsigned bank_quotient(opxy::u32 value) {
    const unsigned low=bank_byte_zeros[value&255];
    return low==8 ? 8+bank_byte_zeros[(value>>8)&255] : low;
}
template<unsigned Order>
inline void bank_decode_block(opxy::u32 descriptor,unsigned start,unsigned count,opxy::i16* output,BankStream* stream) {
    unsigned bit=descriptor&0x3fffff;
    const unsigned k=(descriptor>>22)&31,end=start+count;
    const opxy::u32 mask=(1u<<k)-1;
    opxy::i32 value=0,first=0,second=0,third=0;
    const bool resume=stream&&start;
    if(resume) {
        value=stream->history[0];first=stream->history[1];second=stream->history[2];third=stream->history[3];
    } else {
        opxy::i32 seeds[4]={};
        for(unsigned position=0;position<Order;++position) {
            const opxy::u32 encoded=bank_window(bit)&16383;bit+=14;
            seeds[position]=static_cast<opxy::i32>(encoded&8191)-static_cast<opxy::i32>(encoded&8192);
            if(position>=start&&position<end)output[position-start]=static_cast<opxy::i16>(seeds[position]);
        }
        value=seeds[0];
        if(Order>=2){value=seeds[1];first=seeds[1]-seeds[0];}
        if(Order>=3){value=seeds[2];first=seeds[2]-seeds[1];second=seeds[2]-2*seeds[1]+seeds[0];}
        if(Order==4){value=seeds[3];first=seeds[3]-seeds[2];second=seeds[3]-2*seeds[2]+seeds[1];third=seeds[3]-3*seeds[2]+3*seeds[1]-seeds[0];}
    }
    for(unsigned position=resume?start:Order;position<end;++position) {
        const opxy::u32 view=bank_window(bit);
        const unsigned quotient=bank_quotient(view);
        const opxy::u32 encoded=(quotient<<k)|((view>>(quotient+1))&mask);
        bit+=quotient+1+k;
        const opxy::i32 magnitude=static_cast<opxy::i32>(encoded>>1);
        opxy::i32 delta=magnitude ^ -static_cast<opxy::i32>(encoded&1);
        if(Order==4){third+=delta;delta=third;}
        if(Order>=3){second+=delta;delta=second;}
        if(Order>=2){first+=delta;delta=first;}
        value+=delta;
        if(position>=start)output[position-start]=static_cast<opxy::i16>(value);
    }
    if(stream) {
        stream->control=(descriptor&~0x3fffffu)|bit;
        stream->history[0]=value;stream->history[1]=first;stream->history[2]=second;stream->history[3]=third;
    }
}
// Coefficients are frozen signed18-bit Q14 integers. Their absolute sum is
// at most15*16384, so every product and partial sum fits signed32 bits for
// all authored i16 history values. Exact Rice residuals restore every sample.
template<unsigned Order>
inline void bank_decode_linear(opxy::u32 descriptor,unsigned start,unsigned count,opxy::i16* output,BankStream* stream) {
    unsigned bit=descriptor&0x3fffff;
    const unsigned k=(descriptor>>22)&31,end=start+count;
    const opxy::u32 mask=(1u<<k)-1;
    opxy::i32 history[Order],coefficients[Order];
    const bool resume=stream&&start;
    if(resume) {
        for(unsigned index=0;index<Order;++index){history[index]=stream->history[index];coefficients[index]=stream->coefficients[index];}
    } else {
        for(unsigned position=0;position<Order;++position) {
            const opxy::u32 encoded=bank_window(bit)&16383;bit+=14;
            history[position]=static_cast<opxy::i32>(encoded&8191)-static_cast<opxy::i32>(encoded&8192);
            if(position>=start&&position<end)output[position-start]=static_cast<opxy::i16>(history[position]);
        }
        for(unsigned index=0;index<Order;++index) {
            const opxy::u32 encoded=bank_window(bit)&262143;bit+=18;
            coefficients[index]=static_cast<opxy::i32>(encoded&131071)-static_cast<opxy::i32>(encoded&131072);
        }
    }
    for(unsigned position=resume?start:Order;position<end;++position) {
        const opxy::u32 view=bank_window(bit);
        const unsigned quotient=bank_quotient(view);
        const opxy::u32 encoded=(quotient<<k)|((view>>(quotient+1))&mask);
        bit+=quotient+1+k;
        const opxy::i32 magnitude=static_cast<opxy::i32>(encoded>>1);
        opxy::i32 delta=magnitude ^ -static_cast<opxy::i32>(encoded&1);
        opxy::i32 dot=0;
        for(unsigned index=0;index<Order;++index)dot+=coefficients[index]*history[Order-1-index];
        const opxy::i32 value=dot/16384+delta;
        for(unsigned index=0;index+1<Order;++index)history[index]=history[index+1];
        history[Order-1]=value;
        if(position>=start)output[position-start]=static_cast<opxy::i16>(value);
    }
    if(stream) {
        stream->control=(descriptor&~0x3fffffu)|bit;
        for(unsigned index=0;index<Order;++index){stream->history[index]=history[index];stream->coefficients[index]=coefficients[index];}
    }
}
// A slice stays inside one complete256-sample authored wave. This runs only
// during table preparation; the audio-rate loop reads ordinary i16 buffers.
inline void bank_dispatch(unsigned table,unsigned start,unsigned count,opxy::i16* output,BankStream* stream) {
    const opxy::u32 descriptor=stream&&start?stream->control:bank_blocks[table];
    const unsigned order=(descriptor>>27)&3;
    if(descriptor&(1u<<29)) {
        if(order==1)bank_decode_linear<2>(descriptor,start,count,output,stream);
        else if(order==2)bank_decode_linear<3>(descriptor,start,count,output,stream);
        else bank_decode_linear<4>(descriptor,start,count,output,stream);
    } else {
        if(order==0)bank_decode_block<1>(descriptor,start,count,output,stream);
        else if(order==1)bank_decode_block<2>(descriptor,start,count,output,stream);
        else if(order==2)bank_decode_block<3>(descriptor,start,count,output,stream);
        else bank_decode_block<4>(descriptor,start,count,output,stream);
    }
}
inline void bank_decode(unsigned table,unsigned start,unsigned count,opxy::i16* output) {
    bank_dispatch(table,start,count,output,nullptr);
}
inline void bank_resume(unsigned table,unsigned start,opxy::i16* output,BankStream& stream) {
    bank_dispatch(table,start,16,output,&stream);
}
inline opxy::i32 bank_value(unsigned table,unsigned sample) {
    opxy::i16 value=0;bank_decode(table,sample,1,&value);return value;
}
const opxy::i16 sine[257]={0,804,1608,2410,3212,4011,4808,5602,6393,7179,7962,8739,9512,10278,11039,11793,12539,13279,14010,14732,15446,16151,16846,17530,18204,18868,19519,20159,20787,21403,22005,22594,23170,23731,24279,24811,25329,25832,26319,26790,27245,27683,28105,28510,28898,29268,29621,29956,30273,30571,30852,31113,31356,31580,31785,31971,32137,32285,32412,32521,32609,32678,32728,32757,32767,32757,32728,32678,32609,32521,32412,32285,32137,31971,31785,31580,31356,31113,30852,30571,30273,29956,29621,29268,28898,28510,28105,27683,27245,26790,26319,25832,25329,24811,24279,23731,23170,22594,22005,21403,20787,20159,19519,18868,18204,17530,16846,16151,15446,14732,14010,13279,12539,11793,11039,10278,9512,8739,7962,7179,6393,5602,4808,4011,3212,2410,1608,804,0,-804,-1608,-2410,-3212,-4011,-4808,-5602,-6393,-7179,-7962,-8739,-9512,-10278,-11039,-11793,-12539,-13279,-14010,-14732,-15446,-16151,-16846,-17530,-18204,-18868,-19519,-20159,-20787,-21403,-22005,-22594,-23170,-23731,-24279,-24811,-25329,-25832,-26319,-26790,-27245,-27683,-28105,-28510,-28898,-29268,-29621,-29956,-30273,-30571,-30852,-31113,-31356,-31580,-31785,-31971,-32137,-32285,-32412,-32521,-32609,-32678,-32728,-32757,-32767,-32757,-32728,-32678,-32609,-32521,-32412,-32285,-32137,-31971,-31785,-31580,-31356,-31113,-30852,-30571,-30273,-29956,-29621,-29268,-28898,-28510,-28105,-27683,-27245,-26790,-26319,-25832,-25329,-24811,-24279,-23731,-23170,-22594,-22005,-21403,-20787,-20159,-19519,-18868,-18204,-17530,-16846,-16151,-15446,-14732,-14010,-13279,-12539,-11793,-11039,-10278,-9512,-8739,-7962,-7179,-6393,-5602,-4808,-4011,-3212,-2410,-1608,-804,0};
}
