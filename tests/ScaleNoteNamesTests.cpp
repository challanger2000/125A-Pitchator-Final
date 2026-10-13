#include "../src/ScaleNoteNames.h"
#include <cstdio>
#include <string>
#include <vector>
using namespace pitchator;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"ScaleNoteNames line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int noteClass(const std::string& text){
 if(text.empty())return -1;
 const char* letters="CDEFGAB";
 const int naturals[]={0,2,4,5,7,9,11};
 int pc=-1;
 for(int i=0;i<7;++i)if(text[0]==letters[i]){pc=naturals[i];break;}
 if(pc<0)return -1;
 for(size_t at=1;at<text.size();){
  if(text.compare(at,3,u8"\u266F")==0){++pc;at+=3;}
  else if(text.compare(at,3,u8"\u266D")==0){--pc;at+=3;}
  else if(text.compare(at,1,"#")==0){++pc;++at;}
  else return -1;
 }
 return mod12(pc);
}
int main(){
 CHECK(scaleNoteNames(0,Scale::NaturalMinor)==u8"NOTES: C  \u00B7  D  \u00B7  E\u266D  \u00B7  F  \u00B7  G  \u00B7  A\u266D  \u00B7  B\u266D");
 CHECK(scaleNoteNames(0,Scale::Major)==u8"NOTES: C  \u00B7  D  \u00B7  E  \u00B7  F  \u00B7  G  \u00B7  A  \u00B7  B");
 CHECK(scaleNoteNames(0,Scale::HarmonicMinor)==u8"NOTES: C  \u00B7  D  \u00B7  E\u266D  \u00B7  F  \u00B7  G  \u00B7  A\u266D  \u00B7  B");
 CHECK(scaleNoteNames(5,Scale::Major)==u8"NOTES: F  \u00B7  G  \u00B7  A  \u00B7  B\u266D  \u00B7  C  \u00B7  D  \u00B7  E");
 CHECK(scaleNoteNames(9,Scale::MinorPentatonic)==u8"NOTES: A  \u00B7  C  \u00B7  D  \u00B7  E  \u00B7  G");
 CHECK(scaleNoteNames(0,Scale::Chromatic).find(u8"C\u266F")!=std::string::npos);
 CHECK(scaleNoteNames(-1,Scale::Major)==u8"NOTES: \u2014");
 const std::string sep=u8"  \u00B7  ";
 for(int key=0;key<12;++key){
  for(int scale=0;scale<static_cast<int>(Scale::Count);++scale){
   const auto mode=static_cast<Scale>(scale);
   const std::string names=scaleNoteNames(key,mode);
   CHECK(names.rfind("NOTES: ",0)==0&&names.find(u8"\u2014")==std::string::npos);
   std::string rest=names.substr(7);
   std::vector<std::string> each;
   for(size_t pos=0;;){
    const size_t next=rest.find(sep,pos);
    each.push_back(rest.substr(pos,next==std::string::npos?
                       std::string::npos:next-pos));
    if(next==std::string::npos)break;
    pos=next+sep.size();
   }
   int count=0;
   for(int offset=0;offset<12;++offset){
    if(!isInScale(mode,offset))continue;
    CHECK(count<static_cast<int>(each.size()));
    CHECK(noteClass(each[count])==mod12(key+offset));
    ++count;
   }
   CHECK(count==static_cast<int>(each.size()));
  }
 }
 std::puts("Scale notes spelling: 12 keys x 11 modes PASS");
}
