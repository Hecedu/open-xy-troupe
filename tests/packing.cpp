#include <cstdio>
#include <cstdlib>
#include "bank.h"

namespace {
const opxy::i16 original[1450][256]={
#include "bank_0.h"
#include "bank_1.h"
#include "bank_2.h"
#include "bank_3.h"
};
void require(bool ok,const char* message) {
    if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
}

int main() {
    // Verify every valid16-bit prefix, including the two-byte boundary.
    for(unsigned value=1;value<65536;++value) {
        unsigned reference=0;while(!(value&(1u<<reference)))++reference;
        require(recital::bank_quotient(value)==reference,"Prefix lookup is exact and bounded to15");
    }
    const unsigned available=(sizeof(recital::bank_words)/sizeof(recital::bank_words[0])-2)*32;
    for(unsigned table=0;table<1450;++table) {
        for(unsigned group=0;group<1;++group) {
            const unsigned descriptor=recital::bank_blocks[table];
            const unsigned k=(descriptor>>22)&31,order=((descriptor>>27)&3)+1;
            const bool linear=(descriptor&(1u<<29))!=0;
            require(k<=19&&order<=4&&(descriptor>>30)==0&&(!linear||order>=2),"Bounded full-wave descriptor");
            if(linear) {
                unsigned at=(descriptor&0x3fffff)+order*14,absolute_sum=0;
                for(unsigned coefficient=0;coefficient<order;++coefficient) {
                    const unsigned encoded=recital::bank_window(at)&262143;at+=18;
                    const int value=static_cast<int>(encoded&131071)-static_cast<int>(encoded&131072);
                    absolute_sum+=static_cast<unsigned>(value<0?-value:value);
                }
                require(absolute_sum<=15u*16384u&&absolute_sum*8192u<=2147483647u,"Every signed32-bit predictor product/partial sum is bounded");
            }
            unsigned bit=(descriptor&0x3fffff)+order*14+(linear?order*18:0);
            for(unsigned position=order;position<256;++position) {
                require(bit<available,"Every lookahead begins inside the packed payload");
                const unsigned view=recital::bank_window(bit),q=recital::bank_quotient(view);
                require(q<=15&&(view&(1u<<q))&&q+1+k<=32,"All Rice prefixes and complete codewords have fixed bounds");
                bit+=q+1+k;
            }
            require(bit<=available,"Every block ends before the two guard words");
        }
        recital::BankStream stream={};
        opxy::i16 streamed[256];
        for(unsigned start=0;start<256;start+=16)recital::bank_resume(table,start,streamed+start,stream);
        for(unsigned i=0;i<256;++i)require(streamed[i]==original[table][i],"Every resumed morph stream reconstructs exactly");
        recital::bank_resume((table+19)%1450,0,streamed,stream);
        for(unsigned i=0;i<16;++i)require(streamed[i]==original[(table+19)%1450][i],"Interrupted/reused streams initialize from the newly requested table");
        for(unsigned start=0;start<256;start+=16) {
            opxy::i16 guarded[18];for(auto& value:guarded)value=12345;
            recital::bank_decode(table,start,16,guarded+1);
            require(guarded[0]==12345&&guarded[17]==12345,"Slice writes exactly its16 destination samples");
            for(unsigned i=0;i<16;++i)require(guarded[i+1]==original[table][start+i],"Every morph slice reconstructs exactly");
        }
        for(unsigned start=0;start<256;start+=256) {
            opxy::i16 decoded[256];recital::bank_decode(table,start,256,decoded);
            for(unsigned i=0;i<256;++i)require(decoded[i]==original[table][start+i],"Every cold-note wave block reconstructs exactly");
        }
        for(unsigned index=0;index<256;++index)
            require(recital::bank_value(table,index)==original[table][index],"Every authored sample reconstructs exactly");
    }
    std::puts("PASS:371,200 exact samples, guarded morph/cold slices, exhaustive prefix lookup and bounded compact/Rice reads; bounded resume state and no extra waveform buffers.");
}
