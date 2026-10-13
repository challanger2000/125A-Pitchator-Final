#include "../src/PitchPointEdit.h"
#include <cstdio>
#include <cmath>
#include <string>
using namespace pitchator;
#define CHECK(v) do{if(!(v)){std::printf("FAIL %d %s\n",__LINE__,#v);return 1;}}while(0)
int main(){
 int32_t tick=-1;
 CHECK(edit::parsePosition("1.1.000",16,tick)&&tick==0);
 CHECK(edit::parsePosition("2.3.480",16,tick)&&tick==kTicksPerBar+2*kTicksPerQuarter+480);
 CHECK(edit::parsePosition("16.4.840",16,tick)&&tick==16*kTicksPerBar-120);
 CHECK(edit::parsePosition("1.1.120",16,tick)&&tick==120); // 1/32
 CHECK(!edit::parsePosition("1.1.960",16,tick));
 CHECK(!edit::parsePosition("17.1.0",16,tick));
 CHECK(!edit::parsePosition("0.1.0",16,tick));
 CHECK(!edit::parsePosition("1.5.0",16,tick));
 CHECK(!edit::parsePosition("1.1.-1",16,tick));
 CHECK(!edit::parsePosition("1.1.1garbage",16,tick));
 CHECK(!edit::parsePosition("",16,tick));
 char position[32]={};
 edit::formatPosition(kTicksPerBar+2*kTicksPerQuarter+120,position);
 CHECK(std::string(position)=="2.3.120");
 double st=0;
 CHECK(edit::parseSemitones("+3.25",st)&&st==3.25);
 CHECK(edit::parseSemitones("-2,50",st)&&st==-2.5);
 CHECK(edit::parseSemitones("-48",st)&&st==-48);
 CHECK(!edit::parseSemitones("49",st));
 CHECK(!edit::parseSemitones("nan",st));
 CHECK(!edit::parseSemitones("3st",st));
 CHECK(!edit::parseSemitones("",st));
 char pitch[32]={};
 edit::formatSemitones(-2.5,pitch);
 CHECK(std::string(pitch)=="-2.50");
 std::puts("Exact point-position and semitone parsing PASS");
}
