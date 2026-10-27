#include "../src/shared/FireteamFeedbackQueue.h"
#include <assert.h>
#include <limits>
#include <stdio.h>
int main()
{
    FTFeedbackRules::RecentQueue q;
    assert(q.Pop(0)==0);
    q.Push(1,0); q.Push(2,0.2f); q.Push(1,0.4f);
    assert(q.Pop(0.5f)==1); assert(q.Pop(0.5f)==2); assert(q.Pop(0.5f)==0);
    for(unsigned char i=1;i<=8;++i) q.Push(i,1);
    for(unsigned char i=5;i<=8;++i) assert(q.Pop(2)==i);
    assert(q.Pop(2)==0);
    q.Push(1,0); q.Push(2,3); assert(q.Pop(3)==2); assert(q.Pop(3)==0);
    q.Push(1,10); assert(q.Pop(1)==0); // map clock rolled back
    q.Push(0,1); q.Push(1,std::numeric_limits<float>::quiet_NaN()); assert(q.Pop(1)==0);
    q.Push(3,1); q.Clear(); assert(q.Pop(1)==0); // round start preempts kill backlog
    q.Push(3,1); assert(q.Pop(std::numeric_limits<float>::infinity())==0);
    puts("PASS: bounded/coalesced recent feedback, expiry, map reset, round priority");
}
