"""Exercise the generated native adapter with global stock voice indices.

Only the stock pitch buffer and title callback are replaced by host fixtures.
The factory, method table, engine callbacks and fixed pool are the real code.
"""
from pathlib import Path
from types import SimpleNamespace
import json
import subprocess
import sys
import tempfile

from support import SDK, MACHINE
import os, shutil
repo=SDK
sys.path.insert(0,str(repo))
from tooling.adapters import generate_voice
from tooling.definitions import validate_definition

engine=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(__file__).resolve().parents[1]
definition=validate_definition(json.loads((engine/'definition.json').read_text()),'synth')
machine=SimpleNamespace(definition=definition,prefix='troupe',native_name='troupe',identity={'engine':36})
source=r'''
#include <cstdio>
#include <cstdlib>
#include "voice.cpp"
static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static void invoke_start(void* object) {
    const auto table=*static_cast<const void* const**>(object);
    reinterpret_cast<int(*)(void*)>(const_cast<void*>(table[4]))(object);
}
static bool render_note(void* object) {
    const auto table=*static_cast<const void* const**>(object);
    const int parameters[8]={1073741824,1252698794,966367641,1073741824};
    int output[64];bool audible=false;
    for(unsigned block=0;block<64;++block) {
        reinterpret_cast<int(*)(void*,int*,const int*)>(const_cast<void*>(table[2]))(object,output,parameters);
        for(int value:output){require(value>=-805306368&&value<=805306368,"Output headroom");audible|=value!=0;}
    }
    return audible;
}
int main(){
    for(auto& pitch:opxy::platform::test_pitch)pitch=25481307;
    void* objects[256]={};
    for(unsigned index=0;index<256;++index){
        objects[index]=troupe_factory(index);
        if(!objects[index]){std::fprintf(stderr,"FAIL: factory returned null for valid stock voice index %u\n",index);return 1;}
        for(unsigned previous=0;previous<index;++previous)require(objects[previous]!=objects[index],"Stable distinct stock objects");
    }
    require(troupe_factory(256)==nullptr,"Outside-stock index remains rejected");
    const unsigned active[24]={8,23,64,127,128,191,254,255,0,3,11,25,47,51,67,79,91,103,139,151,163,175,199,211};
    for(unsigned index:active)invoke_start(objects[index]);
    for(unsigned index:active)require(render_note(objects[index]),"All 24 sparse global voices render");
    void* extra=objects[19];invoke_start(extra);
    render_note(objects[8]); // Drain the bounded two-block handoff.
    require(!render_note(objects[8]),"An evicted voice stays silent after the handoff");
    troupe_destroy(objects[8]);
    require(render_note(extra),"Late destroy cannot release another voice's lease");
    invoke_start(objects[8]);require(render_note(objects[8]),"Evicted voice can be restruck");
    for(unsigned index=0;index<256;++index)troupe_destroy(objects[index]);
    for(unsigned index:active)require(!render_note(objects[index]),"Destroy invalidates every released handle");
    std::puts("PASS: real native adapter; 256 non-null unique handles; 24 sparse global voices; bounded stealing, late destroy, restrike and silence after release under ASan/UBSan.");
}
'''
with tempfile.TemporaryDirectory(prefix='troupe-native-index-',) as temporary:
    work=Path(temporary)
    generate_voice(work,machine)
    (work/'stock.hpp').write_text('''#pragma once
#include <opxy/types.hpp>
namespace opxy { namespace platform {
static opxy::u32 test_pitch[64];
static const __UINTPTR_TYPE__ kPitchIncrements=reinterpret_cast<__UINTPTR_TYPE__>(test_pitch);
static const __UINTPTR_TYPE__ kVoiceDefaultMethod=0;
}}
''')
    (work/'synth_controls.hpp').write_text('''#pragma once
#include <opxy/types.hpp>
static opxy::u32 sdk_synth_title_slot(opxy::u32,opxy::u32 index){return index;}
''')
    (work/'check.cpp').write_text(source)
    subprocess.run([shutil.which(os.environ.get('CXX','c++')),'-std=c++11','-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(repo/'sdk/include'),'-I'+str(engine),str(work/'check.cpp'),'-o',str(work/'check')],check=True)
    subprocess.run([str(work/'check')],check=True)
