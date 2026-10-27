"""Author one phase-compatible mixed cast per lead, entirely offline."""
from pathlib import Path
import hashlib, json
import numpy as np

ROOT = Path(__file__).resolve().parent

def generate():
    bank = np.array([np.fromstring(line.strip().strip('{},'), sep=',', dtype=np.int32)
                     for part in range(4) for line in (ROOT / f'bank_{part}.h').read_text().splitlines()]).reshape(10,29,5,256)
    magnitudes = np.abs(np.fft.rfft(bank, axis=-1))
    phases = np.concatenate([np.load(ROOT / (name + '_source_phases.npy'))
                             for name in ('vocal_spectra', 'additional_spectra')])
    dispersion = json.loads((ROOT / 'bank-report.json').read_text())['phase_dispersion']
    order = [5,0,8,9,1,2,6,7,3,4]
    members = [[order[(order.index(lead)+offset)%10] for offset in (0,1,-1,2,-2)] for lead in range(10)]
    inverse = [[next(lead for lead in range(10) if members[lead][slot] == body)
                for slot in range(5)] for body in range(10)]
    waves, scales = [], []
    for lead in range(10):
        magnitude = magnitudes[lead]*.4 + sum(magnitudes[k]*.15 for k in members[lead][1:])
        magnitude[...,0] = magnitude[...,-1] = 0
        phase = phases[lead] + dispersion[lead]*np.arange(129,dtype=float)**2
        authored = np.fft.irfft(magnitude*np.exp(1j*phase), n=256, axis=-1)
        scale = min(1., 8190/float(np.max(np.abs(authored))))
        waves.append(np.rint(authored*scale).astype('<i2'))
        scales.append(scale)
    output = np.array(waves).reshape(1450,256)
    unsigned=(output.reshape(-1).astype(np.int32)&16383).astype('<u2')
    bits=np.unpackbits(unsigned.view(np.uint8),bitorder='little').reshape(-1,16)[:,:14].reshape(-1)
    payload=np.packbits(bits,bitorder='little').tobytes()
    words=np.frombuffer(payload,dtype='<u4').tolist()+[0]
    header = '#pragma once\n#include <opxy/audio.hpp>\nnamespace troupe {\nconst opxy::u32 cast_words['+str(len(words))+']={\n'
    header += ''.join(','.join(str(value)+'u' for value in words[at:at+12])+',\n' for at in range(0,len(words),12))
    header += '''};
// Exact signed 14-bit direct lookup with one immutable guard word.
inline opxy::i32 cast_sample(unsigned table,unsigned sample){
    const unsigned bit=(table*256+sample)*14,at=bit>>5,shift=bit&31;
    opxy::u32 value=cast_words[at]>>shift;
    if(shift>18)value|=cast_words[at+1]<<(32-shift);
    value&=16383;
    return static_cast<opxy::i32>(value&8191)-static_cast<opxy::i32>(value&8192);
}
}
'''
    roster = '#pragma once\nnamespace troupe {\n'
    for name, array in [('cast_members',members),('cast_lead_for_body',inverse)]:
        roster += 'const unsigned '+name+'[10][5]={'+','.join('{'+','.join(map(str,row))+'}' for row in array)+'};\n'
    roster += '}\n'
    np.save(ROOT/'cast_waves.npy',output)
    report = dict(schema=1, tables=1450, frames=256, constant_bytes=len(words)*4,
        peak=int(np.abs(output.astype(np.int32)).max()), members=members, lead_weight=.4,
        companion_weights=[.15]*4, scale_per_lead=scales,
        decoded_sha256=hashlib.sha256(output.tobytes()).hexdigest(),
        phase_basis='Original lead source phase plus preserved per-lead dispersion',
        realtime_fft=False, realtime_extra_oscillators=0)
    return header, roster, report

if __name__ == '__main__':
    header, roster, report = generate()
    (ROOT/'cast_bank.h').write_text(header)
    (ROOT/'cast_members.h').write_text(roster)
    (ROOT/'cast-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
