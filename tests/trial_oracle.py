"""Independent reference/output and authored-bank checks for knob trials."""
from pathlib import Path
import hashlib, json, os, shutil, subprocess, tempfile
import numpy as np
from support import SDK, MACHINE

WORKLOAD = r'''
#include <cstdio>
#include <algorithm>
int main(){
    unsigned long long hashes[3]={14695981039346656037ULL,14695981039346656037ULL,14695981039346656037ULL};
    unsigned long long pitch_energy[3]={};
    int p[8]={},out[64];unsigned pitch[64];
    for(unsigned amount=0;amount<3;++amount)for(unsigned type=0;type<10;++type){
        recital::RenderVoice state={};recital::begin_sound(state,37);
        p[1]=static_cast<int>((2*type+1)*2147483647LL/20);
        p[2]=static_cast<int>(amount*2147483647LL/2);p[3]=1073741824;
        std::fill(pitch,pitch+64,recital::bank_pitch[14]+12345);
        for(unsigned block=0;block<2048;++block){
            p[0]=static_cast<int>((block*37u%32768u)*65536u);
            recital::render_sound(state,out,p,pitch);
            for(int value:out)hashes[amount]=(hashes[amount]^static_cast<unsigned>(value))*1099511628211ULL;
            if(block>1024)pitch_energy[amount]+=static_cast<unsigned>(state.pitch_offset[0]<0?-state.pitch_offset[0]:state.pitch_offset[0]);
        }
    }
    for(unsigned i=0;i<3;++i)std::printf("%llu %llu\n",hashes[i],pitch_energy[i]);
}
'''

def main():
    compiler=shutil.which(os.environ.get('CXX','c++'))
    def compile_run(folder, name, source):
        cpp=folder/(name+'.cpp');binary=folder/name;cpp.write_text(source)
        subprocess.run([compiler,'-std=c++11','-O2','-Wall','-Werror','-I'+str(SDK/'sdk/include'),
                        '-I'+str(MACHINE),'-I'+str(MACHINE/'tests'),str(cpp),'-o',str(binary)],check=True)
        return subprocess.check_output([str(binary)])
    with tempfile.TemporaryDirectory(prefix='recital-trial-oracle-') as scratch:
        scratch=Path(scratch)
        candidate=compile_run(scratch,'candidate','#include "engine.cpp"\n'+WORKLOAD)
        approved=compile_run(scratch,'approved','#include "approved_companion_engine.h"\n'+WORKLOAD)
        rows=[list(map(int,row.split())) for row in candidate.decode().splitlines()]
        prior=[list(map(int,row.split())) for row in approved.decode().splitlines()]
        assert len(set(row[0] for row in rows))==3,'Knob amounts must produce different sound'
        if (MACHINE/'cast_bank.h').exists():
            assert rows[0]==prior[0],'Cast zero must retain exact approved solo output'
            assert rows[0][1]==rows[1][1]==rows[2][1],'Cast must not change lead tuning'
            import sys
            sys.path.insert(0,str(MACHINE))
            from generate_cast import generate
            header,roster,report=generate()
            assert header==(MACHINE/'cast_bank.h').read_text()
            assert roster==(MACHINE/'cast_members.h').read_text()
            # Decode production preparations, then compare with independent
            # weighted integer mixing of the literal authored source arrays.
            source=r'''
#include "engine.cpp"
#include <cstdio>
int main(){for(unsigned amount=0;amount<=256;amount+=64)for(unsigned table=0;table<1450;++table){
    recital::WaveKey key={table/145|(amount<<8),(table%145)/5,0,table%5,0};
    recital::BankStream streams[4]={};opxy::i16 wave[256];
    for(unsigned at=0;at<256;at+=16)recital::bank_slice(key,at,wave+at,streams);
    std::fwrite(wave,sizeof(wave),1,stdout);
}}
'''
            actual=np.frombuffer(compile_run(scratch,'tables',source),dtype='<i2').reshape(5,1450,256)
            def array(text):
                return np.array([np.fromstring(row.strip().strip('{},'),sep=',',dtype=np.int32)
                                 for row in text.splitlines() if row.startswith('{')])
            lead=np.concatenate([array((MACHINE/f'bank_{part}.h').read_text()) for part in range(4)])
            cast=np.load(MACHINE/'cast_waves.npy').astype(np.int32)
            for index,amount in enumerate(range(0,257,64)):
                delta=(cast-lead)*amount
                mixed=lead+np.where(delta<0,-((-delta)//256),delta//256)
                assert np.array_equal(actual[index],mixed),'Prepared cast differs from independent authored mixture'
            # A weighted spectral mixture keeps all contributors and removes DC.
            assert np.abs(cast.mean(axis=1)).max()<.1
            assert np.abs(cast).max()<=8191
            print('PASS: exact approved solo across 1310720 samples; unchanged tuning; 1856000 independent prepared-wave samples; deterministic cast authoring.')
        else:
            assert rows[0][1]==0,'Humanity zero must have precise pitch'
            assert rows[2][1]>rows[1][1]>0,'Higher Humanity must increase expressive pitch movement'
            print('PASS: audible independent control amounts; precise zero; increasing expressive pitch movement; 3932160 rendered samples.')
        print(json.dumps({'candidate':rows,'approved':prior},sort_keys=True))

if __name__=='__main__':main()
