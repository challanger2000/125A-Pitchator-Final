#include "../src/CurveMailbox.h"
#include <cstdio>
using namespace pitchator;
#define TEST(x) do{if(!(x)){std::fprintf(stderr,"CurveMailbox fail %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
 Pattern original;TEST(original.setBars(16));
 TEST(original.upsert({960,3,Curve::Smooth}));
 TEST(original.upsert({57600,1,Curve::Step}));
 auto packet=packCurve(original);
 Pattern read;TEST(unpackCurve(packet,read));
 TEST(read.bars()==16&&read.size()==3&&read.at(2).tick==57600);
 packet.points[1].curve=99;TEST(!unpackCurve(packet,read));
 packet=packCurve(original);packet.points[2].tick=960;
 // PCV2 intentionally permits two ordered same-tick nodes: a JUMP.
 TEST(unpackCurve(packet,read)&&read.hasJumps()&&read.size()==3);
 packet.magic=kLegacyCurvePacketMagic;
 TEST(!unpackCurve(packet,read)); // PCV1 never allowed a second knot
 TEST(unpackCurve(packCurve(original),read));
 packet=packCurve(original);
 CurveMailbox channel(original);
 uint64_t revision=0;Pattern snapshot;
 TEST(channel.snapshot(snapshot,revision));
 TEST(revision==2&&snapshot.size()==3);
 channel.publish(read); // retains prior valid state
 TEST(channel.snapshot(snapshot,revision));
 TEST(revision==4&&snapshot.size()==3);
 Pattern clear;clear.setBars(2);channel.publish(clear);
 TEST(channel.snapshot(snapshot,revision));
 TEST(snapshot.size()==1&&snapshot.bars()==2);

 // Reject malformed packets, ensure failure cannot mutate a valid target.
 const auto baseline=packCurve(original);
 Pattern retained=original;
 auto broken=baseline;
 broken.count=0;TEST(!unpackCurve(broken,retained));
 TEST(retained.bars()==16&&retained.size()==3);
 broken=baseline;broken.bars=17;TEST(!unpackCurve(broken,retained));
 broken=baseline;broken.points[1].semitones=100;
 TEST(!unpackCurve(broken,retained));
 broken=baseline;broken.points[1].tick=-4;
 TEST(!unpackCurve(broken,retained));
 broken=baseline;broken.points[0].semitones=1;
 TEST(!unpackCurve(broken,retained));
 // A 512-slot snapshot should not lose the first or last point.
 Pattern full;TEST(full.setBars(16));
 for(int i=1;i<512;++i)TEST(full.upsert({i*120,((i%17)-8)*0.5,Curve::Linear}));
 auto filled=packCurve(full);TEST(filled.count==512);
 Pattern reconstructed;TEST(unpackCurve(filled,reconstructed));
 TEST(reconstructed.size()==512);
 TEST(reconstructed.at(511).tick==full.at(511).tick);
 // PCV2, exact two same-tick nodes in controller packet and RT mailbox.
 Pattern jumped;TEST(jumped.setBars(2));
 TEST(jumped.upsert({3840,6,Curve::Linear}));
 TEST(jumped.addJump({3840,0,Curve::Smooth}));
 auto packet2=packCurve(jumped);
 TEST(packet2.magic==kCurvePacketMagic&&packet2.count==3);
 Pattern restoredJump;TEST(unpackCurve(packet2,restoredJump));
 TEST(restoredJump.size()==3&&restoredJump.hasJumps());
 TEST(restoredJump.at(1).tick==restoredJump.at(2).tick);
 TEST(restoredJump.value(3840)==0&&restoredJump.value(3839)>5.99);
 channel.publish(restoredJump);
 TEST(channel.snapshot(snapshot,revision));
 TEST(snapshot.size()==3&&snapshot.hasJumps()&&snapshot.value(3840)==0);
 // Triple knots and unsorted arrivals are not valid serialized patterns.
 auto triple=packet2;
 triple.count=4;triple.points[3]=triple.points[2];
 TEST(!unpackCurve(triple,retained));
 auto wrongOrder=packet2;
 wrongOrder.points[1].tick=4000;
 TEST(!unpackCurve(wrongOrder,retained));
 // PCV1 old snapshots remain readable, and cannot embed duplicate ticks.
 auto oldPacket=packCurve(original);
 oldPacket.magic=kLegacyCurvePacketMagic;
 TEST(unpackCurve(oldPacket,retained)&&retained.size()==3);
 auto brokenV1=packet2;brokenV1.magic=kLegacyCurvePacketMagic;
 TEST(!unpackCurve(brokenV1,retained));
 std::puts("CurveMailbox snapshot and packet validation PASS");return 0;
}
