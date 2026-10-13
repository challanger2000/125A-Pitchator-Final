#include "../src/DemoTimer.h"
#include <cstdio>

#define CHECK(v) do{if(!(v)){std::fprintf(stderr,"DemoTimerTests line %d: %s\n",__LINE__,#v);return 1;}}while(0)

int main(){
 pitchator::DemoTimer timer;
 CHECK(!timer.enabled());
 CHECK(timer.allowedSamples(true,512)==512);

 timer.enable(2000.0);
 CHECK(timer.allowedSamples(false,4000)==4000);
 CHECK(!timer.expired());

 CHECK(timer.allowedSamples(true,20000)==20000);
 CHECK(timer.allowedSamples(true,39999)==39999);
 CHECK(!timer.expired());
 CHECK(timer.allowedSamples(true,100)==1);
 CHECK(timer.expired());
 CHECK(timer.allowedSamples(true,512)==0);

 pitchator::DemoTimer rateChange;
 rateChange.enable(48000.0);
 CHECK(rateChange.allowedSamples(true,480000)==480000); // 10 s
 rateChange.setSampleRate(96000.0);
 CHECK(rateChange.allowedSamples(true,1920000)==1920000); // +20 s
 CHECK(rateChange.expired());

 std::puts("Pitchator DemoTimer PASS");
 return 0;
}
