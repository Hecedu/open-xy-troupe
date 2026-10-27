#include <cstdio>
#include <cstdlib>
#include "frozen/approved_companion_engine.h"
static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(){
    using namespace recital;
    opxy::i16 wave[256];unsigned long checks=0;
    const unsigned fractions[]={0,1,0x007fffff,0x00800000,0x00fffffe,0x00ffffff};
    for(unsigned table=0;table<1450;++table){
        bank_decode(table,0,256,wave);
        for(unsigned index=0;index<256;++index)for(unsigned fraction:fractions){
            const unsigned phase=(index<<24)|fraction;
            require(singer_sample<true>(wave,phase)==sample(wave,phase),"Lead keeps exact linear interpolation");
            const unsigned expected=(index+(fraction>=0x00800000u?1u:0u))%256;
            require(singer_sample<false>(wave,phase)==wave[expected],"Companions round to nearest sample, including phase wrap");
            ++checks;
        }
    }
    std::printf("PASS: %lu bank/phase cases; exact lead interpolation and nearest companion lookup with unsigned wrap.\n",checks);
}
