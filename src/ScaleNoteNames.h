#pragma once
#include "PitchEngine.h"
#include <string>
namespace pitchator {
// Display-only musical spelling of PitchEngine::isInScale() pitch classes.
// International notes: B = German H; B-flat (B♭) = German B.
inline std::string scaleNoteNames(int key,Scale scale){
 if(key<0||key>11||!validScale(scale))return u8"NOTES: \u2014";
 constexpr int naturals[7]={0,2,4,5,7,9,11};
 constexpr const char* letters[7]={"C","D","E","F","G","A","B"};
 constexpr int majorRootLetters[12]={0,1,1,2,2,3,3,4,5,5,6,6};
 constexpr int minorRootLetters[12]={0,0,1,2,2,3,3,4,4,5,6,6};
 constexpr const char* sharpNames[12]={"C",u8"C\u266F","D",u8"D\u266F","E","F",u8"F\u266F","G",u8"G\u266F","A",u8"A\u266F","B"};
 const bool minorLike=(scale==Scale::NaturalMinor||scale==Scale::HarmonicMinor||
      scale==Scale::MinorPentatonic||scale==Scale::Dorian||
      scale==Scale::Phrygian||scale==Scale::Locrian);
 const int rootLetter=(minorLike?minorRootLetters:majorRootLetters)[key];
 constexpr int pentMajorDegrees[5]={0,1,2,4,5};
 constexpr int pentMinorDegrees[5]={0,2,3,4,6};
 std::string result="NOTES: ";
 int count=0;
 for(int offset=0;offset<12;++offset){
  if(!isInScale(scale,offset))continue;
  if(count)result+=u8"  \u00B7  ";
  const int pc=mod12(key+offset);
  if(scale==Scale::Chromatic){
   result+=sharpNames[pc];
  }else{
   int degree=count;
   if(scale==Scale::MajorPentatonic)degree=pentMajorDegrees[count];
   if(scale==Scale::MinorPentatonic)degree=pentMinorDegrees[count];
   const int letter=(rootLetter+degree)%7;
   int accidental=mod12(pc-naturals[letter]);
   if(accidental>6)accidental-=12;
   result+=letters[letter];
   switch(accidental){
    case -2:result+=u8"\u266D\u266D";break;
    case -1:result+=u8"\u266D";break;
    case 0:break;
    case 1:result+=u8"\u266F";break;
    case 2:result+="##";break; // double sharp in universal font
    default:return u8"NOTES: \u2014";
   }
  }
  ++count;
 }
 return result;
}
} // namespace pitchator
