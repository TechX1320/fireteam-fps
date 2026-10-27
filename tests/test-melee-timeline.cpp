#include "../src/sshell/FireteamMeleeRules.h"
#include <assert.h>
#include <limits>
#include <stdio.h>
int main()
{
    using namespace FTMeleeRules;
    StrikeClock clock;
    assert(!clock.TakeImpact(0) && !clock.Locked(0));
    clock.Begin(10,0.55f,0.65f);
    assert(clock.Locked(10));
    assert(!clock.TakeImpact(10.54f));
    assert(clock.TakeImpact(10.56f));
    assert(!clock.TakeImpact(10.56f));
    assert(clock.Locked(11.19f));
    assert(!clock.Locked(11.21f));
    clock.Begin(20,0.28f,0.22f); clock.Cancel();
    assert(!clock.TakeImpact(21) && !clock.Locked(21));
    clock.Begin(30,0.18f,0.12f);
    assert(clock.TakeImpact(45)); // hitch: one impact, no catch-up damage burst
    assert(!clock.TakeImpact(45) && !clock.Locked(45));
    clock.Begin(50,0,0); assert(clock.TakeImpact(50)); assert(!clock.Locked(50));
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float inf=std::numeric_limits<float>::infinity();
    assert(WindupSeconds(inf,1)==0 && WindupSeconds(0.2f,inf)==0);
    assert(RecoverySeconds(nan,1,0.2f)==0);
    assert(RecoverySeconds(3,1,0.5f)==0.5f);
    assert(!CanConnect(0,0,0,inf,70));
    assert(!CanConnect(0,inf,0,50,70));
    assert(CanConnect(0,70,0,50,70));
    assert(!CanConnect(0,70.1f,0,50,70));
    assert(!CanConnect(40,0,40,50,70));
    assert(SameLife(17,0,17,0,true));
    assert(!SameLife(17,0,18,0,true)); // recycled object handle
    assert(!SameLife(17,0,17,1,true)); // same player, another life
    assert(!SameLife(17,0,17,0,false)); // dead/spectating/unverified
    assert(!SameLife(0,0,0,0,true));
    assert(KeepTarget(100,90)); assert(!KeepTarget(100,40));
    assert(!KeepTarget(-1,1)); assert(!KeepTarget(inf,1));
    clock.Begin(nan,1,1); assert(!clock.TakeImpact(80));
    clock.Begin(80,1,1); assert(!clock.TakeImpact(nan)); assert(clock.TakeImpact(81));
    const int rates[]={10,20,30,60,120};
    for(unsigned rate=0;rate<5;++rate)
    {
        for(unsigned variant=0;variant<3;++variant)
        {
            const float windups[]={0.28f,0.18f,0.55f};
            const float recovery[]={0.22f,0.12f,0.65f};
            clock.Begin(0,windups[variant],recovery[variant]);
            unsigned impacts=0;
            for(int tick=0;tick<rates[rate]*3;++tick)
            {
                const float now=(float)tick/rates[rate];
                if(clock.TakeImpact(now)) { assert(now>=windups[variant]); ++impacts; }
            }
            assert(impacts==1);
        }
    }
    puts("PASS: melee one-shot timing, recovery, cancellation, identity/life guards, five tick rates");
}
