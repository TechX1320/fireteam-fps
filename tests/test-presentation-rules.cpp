#include "../src/shared/FireteamPresentationRules.h"
#include <assert.h>
#include <limits>
#include <stdio.h>
int main()
{
    using namespace FTPresentationRules;
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float inf=std::numeric_limits<float>::infinity();
    assert(RadarRadius(1080,1)==84); assert(RadarRadius(1080,1.75f)==147);
    assert(RadarRadius(240,0.75f)==40.5f); assert(RadarRadius(1080,nan)==84);
    for(int h=480;h<=2160;h+=120)
    for(float scale=0.75f;scale<=1.76f;scale+=0.05f)
    {
        const float radius=RadarRadius((float)h,scale);
        assert(radius>=40 && radius<=148);
        assert(2*radius+30<h); // vertical corner containment
        assert(2*radius+24<640); // minimum tested width
    }
    assert(DamageSide(0,1)==0); assert(DamageSide(1,0)==1);
    assert(DamageSide(0,-1)==2); assert(DamageSide(-1,0)==3);
    assert(DamageSide(0,0)==-1); assert(DamageSide(nan,1)==-1);
    assert(DamageSide(1,inf)==-1);
    assert(Countdown(10,8.2f)==2); assert(Countdown(10,10)==0);
    assert(Countdown(10,20)==0); assert(Countdown(nan,1)==0);
    assert(Countdown(99999,0)==120);
    assert(FTCombatRules::PendingSpawns(36,10,6)==20);
    assert(FTCombatRules::PendingSpawns(36,36,0)==0);
    assert(FTCombatRules::PendingSpawns(36,50,8)==0);
    assert(FTCombatRules::PendingSpawns(10,5,100)==0);
    assert(FTCombatRules::PendingSpawns(0,0,0)==0);
    assert(FTCombatRules::ReloadSeconds(3.2f)==3.2f);
    assert(FTCombatRules::ReloadSeconds(0)==0.125f);
    assert(FTCombatRules::ReloadSeconds(-100)==0.125f);
    assert(FTCombatRules::ReloadSeconds(nan)==0.125f);
    assert(FTCombatRules::ReloadSeconds(inf)==0.125f);
    assert(FTCombatRules::ReloadSeconds(100)==30);
    puts("PASS: radar bounds, directional cues, countdowns, pending counts, authoritative reload bounds");
}
